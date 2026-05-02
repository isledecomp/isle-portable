#include "d3drmrenderer.h"
#include "d3drmrenderer_palettesw.h"
#include "ddsurface_impl.h"
#include "mathutils.h"
#include "meshutils.h"
#include "miniwin.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

struct PalVertexXY {
	float x, y, z, w;
	Uint8 brightness; // 0..LIGHT_LEVELS-1
	float u_over_w, v_over_w;
	float one_over_w;
};

static constexpr int PERSP_STEP = 16;

inline static D3DVECTOR PalSubtract(const D3DVECTOR& a, const D3DVECTOR& b)
{
	return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline static bool PalIsBackface(const D3DVECTOR& a, const D3DVECTOR& b, const D3DVECTOR& c)
{
	D3DVECTOR normal = CrossProduct(PalSubtract(b, a), PalSubtract(c, a));
	return DotProduct(normal, a) >= 0.0f;
}

Direct3DRMPaletteSWRenderer::Direct3DRMPaletteSWRenderer(DWORD width, DWORD height)
{
	m_virtualWidth = width;
	m_virtualHeight = height;

	memset(m_lightLUT, 0, sizeof(m_lightLUT));
	memset(m_blendLUT, 0, sizeof(m_blendLUT));
	ViewportTransform viewportTransform = {1.0f, 0.0f, 0.0f};
	Resize(width, height, viewportTransform);
}

Direct3DRMPaletteSWRenderer::~Direct3DRMPaletteSWRenderer()
{
	SDL_DestroySurface(m_renderedImage);
	if (m_flipPalette) {
		SDL_DestroyPalette(m_flipPalette);
	}
}

static bool PalettesEqual(SDL_Palette* a, SDL_Palette* b)
{
	if (!a || !b || a->ncolors != b->ncolors) {
		return false;
	}
	return memcmp(a->colors, b->colors, a->ncolors * sizeof(SDL_Color)) == 0;
}

// ---------------------------------------------------------------------------
// Lighting LUT
// ---------------------------------------------------------------------------
// For each palette entry and brightness level, precompute the closest palette
// index.  Brightness 0 = black, LIGHT_LEVELS-1 = full colour.
// This avoids per-pixel RGB maths entirely — the rasteriser just does:
//   outPixel = m_lightLUT[texel * LIGHT_LEVELS + brightness]
// ---------------------------------------------------------------------------

void Direct3DRMPaletteSWRenderer::BuildLightingLUT()
{
	// Use m_flipPalette (snapshot from Flip time) if available — that's the
	// palette actually sent to the VGA DAC.  Fall back to m_palette for the
	// first frame before any Flip has occurred.
	SDL_Palette* pal = m_flipPalette ? m_flipPalette : m_palette;
	if (!pal) {
		return;
	}

	const SDL_Color* colors = pal->colors;
	const int ncolors = pal->ncolors;

	for (int idx = 0; idx < 256; ++idx) {
		int sr, sg, sb;
		if (idx < ncolors) {
			sr = colors[idx].r;
			sg = colors[idx].g;
			sb = colors[idx].b;
		}
		else {
			sr = sg = sb = 0;
		}

		for (int lev = 0; lev < LIGHT_LEVELS; ++lev) {
			// Target colour at this brightness
			int tr = (sr * lev) / (LIGHT_LEVELS - 1);
			int tg = (sg * lev) / (LIGHT_LEVELS - 1);
			int tb = (sb * lev) / (LIGHT_LEVELS - 1);

			// Find nearest palette entry (redmean perceptual distance)
			int bestDist = INT_MAX;
			Uint8 bestIdx = static_cast<Uint8>(idx);
			for (int c = 0; c < ncolors; ++c) {
				int dr = colors[c].r - tr;
				int dg = colors[c].g - tg;
				int db = colors[c].b - tb;
				int rmean = (tr + colors[c].r) / 2;
				int dist = ((512 + rmean) * dr * dr >> 8) + 4 * dg * dg + ((767 - rmean) * db * db >> 8);
				if (dist < bestDist) {
					bestDist = dist;
					bestIdx = static_cast<Uint8>(c);
					if (dist == 0) {
						break;
					}
				}
			}

			m_lightLUT[idx * LIGHT_LEVELS + lev] = bestIdx;
		}
	}

	m_lightLUTDirty = false;
}

void Direct3DRMPaletteSWRenderer::BuildBlendLUT()
{
	SDL_Palette* pal = m_flipPalette ? m_flipPalette : m_palette;
	if (!pal) {
		memset(m_blendLUT, 0, sizeof(m_blendLUT));
		return;
	}

	const SDL_Color* colors = pal->colors;
	const int ncolors = pal->ncolors;

	for (int a = 0; a < 256; ++a) {
		int ar, ag, ab;
		if (a < ncolors) {
			ar = colors[a].r;
			ag = colors[a].g;
			ab = colors[a].b;
		}
		else {
			ar = ag = ab = 0;
		}

		for (int b = 0; b < 256; ++b) {
			int br, bg, bb;
			if (b < ncolors) {
				br = colors[b].r;
				bg = colors[b].g;
				bb = colors[b].b;
			}
			else {
				br = bg = bb = 0;
			}

			// 50/50 blend
			int tr = (ar + br) >> 1;
			int tg = (ag + bg) >> 1;
			int tb = (ab + bb) >> 1;

			// Find nearest palette entry
			int bestDist = INT_MAX;
			Uint8 bestIdx = 0;
			for (int c = 0; c < ncolors; ++c) {
				int dr = colors[c].r - tr;
				int dg = colors[c].g - tg;
				int db = colors[c].b - tb;
				int rmean = (tr + colors[c].r) / 2;
				int dist = ((512 + rmean) * dr * dr >> 8) + 4 * dg * dg + ((767 - rmean) * db * db >> 8);
				if (dist < bestDist) {
					bestDist = dist;
					bestIdx = static_cast<Uint8>(c);
					if (dist == 0) {
						break;
					}
				}
			}

			m_blendLUT[a * 256 + b] = bestIdx;
		}
	}
}

void Direct3DRMPaletteSWRenderer::PushLights(const SceneLight* lights, size_t count)
{
	m_lights.assign(lights, lights + count);
}

void Direct3DRMPaletteSWRenderer::SetFrustumPlanes(const Plane* frustumPlanes)
{
	memcpy(m_frustumPlanes, frustumPlanes, sizeof(m_frustumPlanes));
}

void Direct3DRMPaletteSWRenderer::SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back)
{
	m_front = front;
	m_back = back;
	memcpy(m_projection, projection, sizeof(D3DRMMATRIX4D));
}

void Direct3DRMPaletteSWRenderer::ClearZBuffer()
{
	static_assert(sizeof(float) == sizeof(uint32_t), "float must be 32-bit");
	const size_t size = m_zBuffer.size();
	uint32_t* dst = reinterpret_cast<uint32_t*>(m_zBuffer.data());
	for (size_t i = 0; i < size; ++i) {
		dst[i] = 0x7F800000u;
	}
}

void Direct3DRMPaletteSWRenderer::ProjectVertex(const D3DVECTOR& v, D3DRMVECTOR4D& p) const
{
	float px = m_projection[0][0] * v.x + m_projection[1][0] * v.y + m_projection[2][0] * v.z + m_projection[3][0];
	float py = m_projection[0][1] * v.x + m_projection[1][1] * v.y + m_projection[2][1] * v.z + m_projection[3][1];
	float pz = m_projection[0][2] * v.x + m_projection[1][2] * v.y + m_projection[2][2] * v.z + m_projection[3][2];
	float pw = m_projection[0][3] * v.x + m_projection[1][3] * v.y + m_projection[2][3] * v.z + m_projection[3][3];

	p.w = pw;

	// Perspective divide
	if (pw != 0.0f) {
		float invW = 1.0f / pw;
		px *= invW;
		py *= invW;
		pz *= invW;
	}

	// Map from NDC [-1,1] to screen coordinates
	p.x = (px * 0.5f + 0.5f) * m_width;
	p.y = (1.0f - (py * 0.5f + 0.5f)) * m_height;
	p.z = pz;
}

// ---------------------------------------------------------------------------
// Lighting — returns a brightness level 0..LIGHT_LEVELS-1
// ---------------------------------------------------------------------------

// Fast integer-based pow approximation for specular highlights.
// Repeated squaring: computes base^exp where exp is a positive integer.
// Good enough for 8-bit paletted lighting, avoids expensive FPU std::pow.
inline static float FastPow(float base, float exponent)
{
	if (base <= 0.0f) {
		return 0.0f;
	}
	int iexp = static_cast<int>(exponent + 0.5f);
	if (iexp <= 0) {
		return 1.0f;
	}
	float result = 1.0f;
	float b = base;
	while (iexp > 0) {
		if (iexp & 1) {
			result *= b;
		}
		b *= b;
		iexp >>= 1;
	}
	return result;
}

Uint8 Direct3DRMPaletteSWRenderer::ApplyLighting(
	const D3DVECTOR& position,
	const D3DVECTOR& normal,
	const Appearance& appearance,
	Uint8 texel
)
{
	(void) texel; // brightness is independent of the palette index

	float intensity = 0.0f;

	D3DVECTOR n = Normalize(TransformNormal(normal, m_normalMatrix));

	for (const auto& light : m_lights) {
		if (light.positional == 0.0f && light.directional == 0.0f) {
			// Ambient
			float lum = light.color.r * 0.299f + light.color.g * 0.587f + light.color.b * 0.114f;
			intensity += lum;
			continue;
		}

		// Precompute luminance once per light (avoids redundant multiplies)
		float lum = light.color.r * 0.299f + light.color.g * 0.587f + light.color.b * 0.114f;

		D3DVECTOR lightVec;
		if (light.directional == 1.0f) {
			lightVec = {-light.direction.x, -light.direction.y, -light.direction.z};
		}
		else {
			lightVec = {light.position.x - position.x, light.position.y - position.y, light.position.z - position.z};
		}
		lightVec = Normalize(lightVec);

		float dotNL = DotProduct(n, lightVec);
		if (dotNL > 0.0f) {
			intensity += dotNL * lum;

			// Specular — use fast integer pow instead of std::pow
			if (appearance.shininess > 0.0f && light.directional == 1.0f) {
				D3DVECTOR viewVec = Normalize({-position.x, -position.y, -position.z});
				D3DVECTOR H = Normalize({lightVec.x + viewVec.x, lightVec.y + viewVec.y, lightVec.z + viewVec.z});
				float dotNH = std::max(DotProduct(n, H), 0.0f);
				float spec = FastPow(dotNH, appearance.shininess);
				intensity += spec * lum;
			}
		}
	}

	intensity = std::min(intensity, 1.0f);
	int level = static_cast<int>(intensity * (LIGHT_LEVELS - 1) + 0.5f);
	if (level < 0) {
		level = 0;
	}
	if (level >= LIGHT_LEVELS) {
		level = LIGHT_LEVELS - 1;
	}
	return static_cast<Uint8>(level);
}

static D3DRMVERTEX PalSplitEdge(D3DRMVERTEX a, const D3DRMVERTEX& b, float plane)
{
	float t = (plane - a.position.z) / (b.position.z - a.position.z);
	a.position.x += t * (b.position.x - a.position.x);
	a.position.y += t * (b.position.y - a.position.y);
	a.position.z = plane;

	a.texCoord.u += t * (b.texCoord.u - a.texCoord.u);
	a.texCoord.v += t * (b.texCoord.v - a.texCoord.v);

	a.normal.x += t * (b.normal.x - a.normal.x);
	a.normal.y += t * (b.normal.y - a.normal.y);
	a.normal.z += t * (b.normal.z - a.normal.z);

	a.normal = Normalize(a.normal);

	return a;
}

static bool PalIsTriangleOutsideViewCone(
	const D3DVECTOR& v0,
	const D3DVECTOR& v1,
	const D3DVECTOR& v2,
	const Plane* frustumPlanes
)
{
	for (int i = 0; i < 4; ++i) {
		const Plane& plane = frustumPlanes[i];

		float d0 = DotProduct(plane.normal, v0) + plane.d;
		float d1 = DotProduct(plane.normal, v1) + plane.d;
		float d2 = DotProduct(plane.normal, v2) + plane.d;

		if (d0 < 0 && d1 < 0 && d2 < 0) {
			return true;
		}
	}
	return false;
}

void Direct3DRMPaletteSWRenderer::DrawTriangleClipped(const D3DRMVERTEX (&v)[3], const Appearance& appearance)
{
	bool in0 = v[0].position.z >= m_front;
	bool in1 = v[1].position.z >= m_front;
	bool in2 = v[2].position.z >= m_front;

	int insideCount = in0 + in1 + in2;

	if (insideCount == 0 || (v[0].position.z > m_back && v[1].position.z > m_back && v[2].position.z > m_back)) {
		return;
	}
	if (PalIsTriangleOutsideViewCone(v[0].position, v[1].position, v[2].position, m_frustumPlanes)) {
		return;
	}

	if (insideCount == 3) {
		DrawTriangleProjected(v[0], v[1], v[2], appearance);
	}
	else if (insideCount == 2) {
		D3DRMVERTEX split;
		if (!in0) {
			split = PalSplitEdge(v[2], v[0], m_front);
			DrawTriangleProjected(v[1], v[2], split, appearance);
			DrawTriangleProjected(v[1], split, PalSplitEdge(v[1], v[0], m_front), appearance);
		}
		else if (!in1) {
			split = PalSplitEdge(v[0], v[1], m_front);
			DrawTriangleProjected(v[2], v[0], split, appearance);
			DrawTriangleProjected(v[2], split, PalSplitEdge(v[2], v[1], m_front), appearance);
		}
		else {
			split = PalSplitEdge(v[1], v[2], m_front);
			DrawTriangleProjected(v[0], v[1], split, appearance);
			DrawTriangleProjected(v[0], split, PalSplitEdge(v[0], v[2], m_front), appearance);
		}
	}
	else if (in0) {
		DrawTriangleProjected(v[0], PalSplitEdge(v[0], v[1], m_front), PalSplitEdge(v[0], v[2], m_front), appearance);
	}
	else if (in1) {
		DrawTriangleProjected(PalSplitEdge(v[1], v[0], m_front), v[1], PalSplitEdge(v[1], v[2], m_front), appearance);
	}
	else {
		DrawTriangleProjected(PalSplitEdge(v[2], v[0], m_front), PalSplitEdge(v[2], v[1], m_front), v[2], appearance);
	}
}

void Direct3DRMPaletteSWRenderer::DrawTriangleProjected(
	const D3DRMVERTEX& v0,
	const D3DRMVERTEX& v1,
	const D3DRMVERTEX& v2,
	const Appearance& appearance
)
{
	if (PalIsBackface(v0.position, v1.position, v2.position)) {
		return;
	}

	D3DRMVECTOR4D p0, p1, p2;
	ProjectVertex(v0.position, p0);
	ProjectVertex(v1.position, p1);
	ProjectVertex(v2.position, p2);

	Uint8 b0 = ApplyLighting(v0.position, v0.normal, appearance, 0);
	Uint8 b1 = b0, b2 = b0;
	if (!appearance.flat) {
		b1 = ApplyLighting(v1.position, v1.normal, appearance, 0);
		b2 = ApplyLighting(v2.position, v2.normal, appearance, 0);
	}

	Uint8* pixels = static_cast<Uint8*>(m_renderedImage->pixels);
	int pitch = m_renderedImage->pitch;

	PalVertexXY verts[3] = {
		{p0.x, p0.y, p0.z, p0.w, b0, 0, 0, 0},
		{p1.x, p1.y, p1.z, p1.w, b1, 0, 0, 0},
		{p2.x, p2.y, p2.z, p2.w, b2, 0, 0, 0},
	};

	Uint32 textureId = appearance.textureId;
	int texturePitch = 0;
	Uint8* texels = nullptr;
	int texWidthScale = 0;
	int texHeightScale = 0;

	if (textureId != NO_TEXTURE_ID) {
		SDL_Surface* texture = m_textures[textureId].cached;
		if (texture) {
			texturePitch = texture->pitch;
			texels = static_cast<Uint8*>(texture->pixels);
			texWidthScale = texture->w - 1;
			texHeightScale = texture->h - 1;
		}

		verts[0].u_over_w = v0.texCoord.u / p0.w;
		verts[0].v_over_w = v0.texCoord.v / p0.w;
		verts[0].one_over_w = 1.0f / p0.w;

		verts[1].u_over_w = v1.texCoord.u / p1.w;
		verts[1].v_over_w = v1.texCoord.v / p1.w;
		verts[1].one_over_w = 1.0f / p1.w;

		verts[2].u_over_w = v2.texCoord.u / p2.w;
		verts[2].v_over_w = v2.texCoord.v / p2.w;
		verts[2].one_over_w = 1.0f / p2.w;
	}

	// Sort verts
	if (verts[0].y > verts[1].y) {
		std::swap(verts[0], verts[1]);
	}
	if (verts[1].y > verts[2].y) {
		std::swap(verts[1], verts[2]);
	}
	if (verts[0].y > verts[1].y) {
		std::swap(verts[0], verts[1]);
	}

	int minY = std::max(0, static_cast<int>(std::ceil(verts[0].y)));
	int maxY = std::min(m_height - 1, static_cast<int>(std::floor(verts[2].y)));

	// For untextured triangles, find the nearest palette entry for the
	// material colour so we can use the LUT.
	Uint8 materialPalIdx = 0;
	if (!texels && m_palette) {
		Uint8 mr = appearance.color.r;
		Uint8 mg = appearance.color.g;
		Uint8 mb = appearance.color.b;
		int bestDist = INT_MAX;
		for (int c = 0; c < m_palette->ncolors; ++c) {
			int dr = m_palette->colors[c].r - mr;
			int dg = m_palette->colors[c].g - mg;
			int db = m_palette->colors[c].b - mb;
			int dist = dr * dr + dg * dg + db * db;
			if (dist < bestDist) {
				bestDist = dist;
				materialPalIdx = static_cast<Uint8>(c);
				if (dist == 0) {
					break;
				}
			}
		}
	}

	Uint8 alpha = appearance.color.a;

	// --- Set up incremental edge stepping ---
	// Long edge: verts[0] -> verts[2] (always the "right" side before swap)
	float longDy = verts[2].y - verts[0].y;
	float invLongDy = (longDy != 0.0f) ? 1.0f / longDy : 0.0f;
	// Long edge values at minY
	float longT0 = (minY - verts[0].y) * invLongDy;
	PalVertexXY longEdge;
	longEdge.x = verts[0].x + longT0 * (verts[2].x - verts[0].x);
	longEdge.z = verts[0].z + longT0 * (verts[2].z - verts[0].z);
	longEdge.u_over_w = verts[0].u_over_w + longT0 * (verts[2].u_over_w - verts[0].u_over_w);
	longEdge.v_over_w = verts[0].v_over_w + longT0 * (verts[2].v_over_w - verts[0].v_over_w);
	longEdge.one_over_w = verts[0].one_over_w + longT0 * (verts[2].one_over_w - verts[0].one_over_w);
	float longBri = verts[0].brightness + longT0 * (static_cast<float>(verts[2].brightness) - verts[0].brightness);
	// Long edge step per scanline
	float longStepX = (verts[2].x - verts[0].x) * invLongDy;
	float longStepZ = (verts[2].z - verts[0].z) * invLongDy;
	float longStepBri = (static_cast<float>(verts[2].brightness) - verts[0].brightness) * invLongDy;
	float longStepUW = (verts[2].u_over_w - verts[0].u_over_w) * invLongDy;
	float longStepVW = (verts[2].v_over_w - verts[0].v_over_w) * invLongDy;
	float longStepOW = (verts[2].one_over_w - verts[0].one_over_w) * invLongDy;

	// Short edge: verts[0]->verts[1] then verts[1]->verts[2]
	// We set up the first segment and re-init at the midpoint.
	float shortBri;
	auto setupShortEdge = [&](const PalVertexXY& a,
							  const PalVertexXY& b,
							  PalVertexXY& edge,
							  float& sBri,
							  float& stepX,
							  float& stepZ,
							  float& stepBri,
							  float& stepUW,
							  float& stepVW,
							  float& stepOW,
							  int startY) {
		float dy = b.y - a.y;
		float invDy = (dy != 0.0f) ? 1.0f / dy : 0.0f;
		float t0 = (startY - a.y) * invDy;
		edge.x = a.x + t0 * (b.x - a.x);
		edge.z = a.z + t0 * (b.z - a.z);
		sBri = a.brightness + t0 * (static_cast<float>(b.brightness) - a.brightness);
		edge.u_over_w = a.u_over_w + t0 * (b.u_over_w - a.u_over_w);
		edge.v_over_w = a.v_over_w + t0 * (b.v_over_w - a.v_over_w);
		edge.one_over_w = a.one_over_w + t0 * (b.one_over_w - a.one_over_w);
		stepX = (b.x - a.x) * invDy;
		stepZ = (b.z - a.z) * invDy;
		stepBri = (static_cast<float>(b.brightness) - a.brightness) * invDy;
		stepUW = (b.u_over_w - a.u_over_w) * invDy;
		stepVW = (b.v_over_w - a.v_over_w) * invDy;
		stepOW = (b.one_over_w - a.one_over_w) * invDy;
	};

	PalVertexXY shortEdge;
	float shortStepX, shortStepZ, shortStepBri, shortStepUW, shortStepVW, shortStepOW;
	int midY = static_cast<int>(std::ceil(verts[1].y));
	bool pastMid = (minY >= midY);
	if (pastMid) {
		setupShortEdge(
			verts[1],
			verts[2],
			shortEdge,
			shortBri,
			shortStepX,
			shortStepZ,
			shortStepBri,
			shortStepUW,
			shortStepVW,
			shortStepOW,
			minY
		);
	}
	else {
		setupShortEdge(
			verts[0],
			verts[1],
			shortEdge,
			shortBri,
			shortStepX,
			shortStepZ,
			shortStepBri,
			shortStepUW,
			shortStepVW,
			shortStepOW,
			minY
		);
	}

	// Precompute material LUT row pointer for untextured triangles
	const Uint8* materialLightRow = texels ? nullptr : &m_lightLUT[materialPalIdx * LIGHT_LEVELS];

	for (int y = minY; y <= maxY; ++y) {
		// Switch to second short edge segment at midpoint
		if (!pastMid && y >= midY) {
			pastMid = true;
			setupShortEdge(
				verts[1],
				verts[2],
				shortEdge,
				shortBri,
				shortStepX,
				shortStepZ,
				shortStepBri,
				shortStepUW,
				shortStepVW,
				shortStepOW,
				y
			);
		}

		// Determine left/right from the two edges
		float lx, lz, lBri, lUW, lVW, lOW;
		float rx, rz, rBri, rUW, rVW, rOW;
		if (shortEdge.x <= longEdge.x) {
			lx = shortEdge.x;
			lz = shortEdge.z;
			lBri = shortBri;
			lUW = shortEdge.u_over_w;
			lVW = shortEdge.v_over_w;
			lOW = shortEdge.one_over_w;
			rx = longEdge.x;
			rz = longEdge.z;
			rBri = longBri;
			rUW = longEdge.u_over_w;
			rVW = longEdge.v_over_w;
			rOW = longEdge.one_over_w;
		}
		else {
			lx = longEdge.x;
			lz = longEdge.z;
			lBri = longBri;
			lUW = longEdge.u_over_w;
			lVW = longEdge.v_over_w;
			lOW = longEdge.one_over_w;
			rx = shortEdge.x;
			rz = shortEdge.z;
			rBri = shortBri;
			rUW = shortEdge.u_over_w;
			rVW = shortEdge.v_over_w;
			rOW = shortEdge.one_over_w;
		}

		int startX = std::max(0, static_cast<int>(std::ceil(lx)));
		int endX = std::min(m_width - 1, static_cast<int>(std::floor(rx)));

		float span = rx - lx;
		if (span <= 0.0f || startX > endX) {
			// Step edges and continue
			shortEdge.x += shortStepX;
			shortEdge.z += shortStepZ;
			shortBri += shortStepBri;
			shortEdge.u_over_w += shortStepUW;
			shortEdge.v_over_w += shortStepVW;
			shortEdge.one_over_w += shortStepOW;
			longEdge.x += longStepX;
			longEdge.z += longStepZ;
			longBri += longStepBri;
			longEdge.u_over_w += longStepUW;
			longEdge.v_over_w += longStepVW;
			longEdge.one_over_w += longStepOW;
			continue;
		}

		float invSpan = 1.0f / span;

		// Precompute per-pixel step values
		float zStep = (rz - lz) * invSpan;
		float startT = (startX - lx) * invSpan;
		float z = lz + startT * (rz - lz);

		// Integer brightness with 8-bit fractional part for stepping
		int briFix = static_cast<int>((lBri + startT * (rBri - lBri)) * 256.0f);
		int briStepFix = static_cast<int>((rBri - lBri) * invSpan * 256.0f);

		Uint8* row = pixels + y * pitch;
		float* zPtr = &m_zBuffer[y * m_width + startX];

		if (texels) {
			// --- Textured scanline with periodic perspective correction ---
			float uow = lUW + startT * (rUW - lUW);
			float vow = lVW + startT * (rVW - lVW);
			float oow = lOW + startT * (rOW - lOW);
			float uowStep = (rUW - lUW) * invSpan;
			float vowStep = (rVW - lVW) * invSpan;
			float oowStep = (rOW - lOW) * invSpan;

			int x = startX;
			while (x <= endX) {
				// Perspective correction at this point
				float inv_w0 = 1.0f / oow;
				float u0 = uow * inv_w0;
				float v0 = vow * inv_w0;

				int remaining = endX - x + 1;
				int blockLen = (remaining > PERSP_STEP) ? PERSP_STEP : remaining;

				// Compute end-of-block perspective-correct UVs
				float uowEnd = uow + uowStep * blockLen;
				float vowEnd = vow + vowStep * blockLen;
				float oowEnd = oow + oowStep * blockLen;

				float inv_w1 = 1.0f / oowEnd;
				float u1 = uowEnd * inv_w1;
				float v1 = vowEnd * inv_w1;

				// Affine step within this block
				float invBlock = (blockLen > 1) ? (1.0f / blockLen) : 0.0f;
				float uAffStep = (u1 - u0) * invBlock;
				float vAffStep = (v1 - v0) * invBlock;
				float uAff = u0;
				float vAff = v0;

				float zLocal = z;
				int briLocal = briFix;
				float* zP = zPtr;

				for (int i = 0; i < blockLen; ++i, ++x) {
					if (zLocal < *zP) {
						int bri = briLocal >> 8;
						if (bri < 0) {
							bri = 0;
						}
						else if (bri >= LIGHT_LEVELS) {
							bri = LIGHT_LEVELS - 1;
						}

						// Fast UV tile: wrap to [0,1)
						float uTile = uAff;
						float vTile = vAff;
						int ui = static_cast<int>(uTile);
						int vi = static_cast<int>(vTile);
						uTile -= ui;
						vTile -= vi;
						if (uTile < 0.0f) {
							uTile += 1.0f;
						}
						if (vTile < 0.0f) {
							vTile += 1.0f;
						}

						int texX = static_cast<int>(uTile * texWidthScale);
						int texY = static_cast<int>(vTile * texHeightScale);

						Uint8 texel = texels[texY * texturePitch + texX];

						Uint8 palIdx = m_lightLUT[texel * LIGHT_LEVELS + bri];
						if (m_transparencyEnabled) {
							row[x] = m_blendLUT[palIdx * 256 + row[x]];
						}
						else {
							*zP = zLocal;
							row[x] = palIdx;
						}
					}
					zLocal += zStep;
					briLocal += briStepFix;
					uAff += uAffStep;
					vAff += vAffStep;
					++zP;
				}

				z = zLocal;
				briFix = briLocal;
				zPtr = zP;
				uow = uowEnd;
				vow = vowEnd;
				oow = oowEnd;
			}
		}
		else {
			// --- Untextured scanline ---
			if (alpha == 0) {
				// Fully transparent material, skip entire scanline
			}
			else {
				for (int x = startX; x <= endX; ++x, ++zPtr, z += zStep, briFix += briStepFix) {
					if (z >= *zPtr) {
						continue;
					}

					int bri = briFix >> 8;
					if (bri < 0) {
						bri = 0;
					}
					else if (bri >= LIGHT_LEVELS) {
						bri = LIGHT_LEVELS - 1;
					}

					Uint8 palIdx = materialLightRow[bri];

					if (m_transparencyEnabled) {
						row[x] = m_blendLUT[palIdx * 256 + row[x]];
					}
					else {
						*zPtr = z;
						row[x] = palIdx;
					}
				}
			}
		}

		// Step both edges to next scanline
		shortEdge.x += shortStepX;
		shortEdge.z += shortStepZ;
		shortBri += shortStepBri;
		shortEdge.u_over_w += shortStepUW;
		shortEdge.v_over_w += shortStepVW;
		shortEdge.one_over_w += shortStepOW;
		longEdge.x += longStepX;
		longEdge.z += longStepZ;
		longBri += longStepBri;
		longEdge.u_over_w += longStepUW;
		longEdge.v_over_w += longStepVW;
		longEdge.one_over_w += longStepOW;
	}
}

struct PalCacheDestroyContext {
	Direct3DRMPaletteSWRenderer* renderer;
	Uint32 id;
};

void Direct3DRMPaletteSWRenderer::AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture)
{
	auto* ctx = new PalCacheDestroyContext{this, id};
	texture->AddDestroyCallback(
		[](IDirect3DRMObject* obj, void* arg) {
			auto* ctx = static_cast<PalCacheDestroyContext*>(arg);
			auto& cacheEntry = ctx->renderer->m_textures[ctx->id];
			if (cacheEntry.cached) {
				// Only free surfaces we own (3D texture duplicates).
				// UI textures point to the original surface — don't free those.
				auto* origTexture = static_cast<Direct3DRMTextureImpl*>(cacheEntry.texture);
				auto* origSurface = static_cast<DirectDrawSurfaceImpl*>(origTexture->m_surface);
				if (cacheEntry.cached != origSurface->m_surface) {
					SDL_UnlockSurface(cacheEntry.cached);
					SDL_DestroySurface(cacheEntry.cached);
				}
				cacheEntry.cached = nullptr;
				cacheEntry.texture = nullptr;
			}
			delete ctx;
		},
		ctx
	);
}

// Build a 256-byte remap table from a texture's own palette to the game
// palette.  For each source index, find the nearest colour in the game
// palette by Euclidean distance in RGB.
static void BuildPaletteRemap(Uint8* remap, SDL_Palette* srcPal, SDL_Palette* dstPal)
{
	if (!srcPal || !dstPal) {
		// Identity if either palette is missing.
		for (int i = 0; i < 256; ++i) {
			remap[i] = static_cast<Uint8>(i);
		}
		return;
	}

	const SDL_Color* sc = srcPal->colors;
	const SDL_Color* dc = dstPal->colors;
	int dn = dstPal->ncolors;

	for (int i = 0; i < 256; ++i) {
		if (i >= srcPal->ncolors) {
			remap[i] = 0;
			continue;
		}
		int sr = sc[i].r, sg = sc[i].g, sb = sc[i].b;
		int bestDist = INT_MAX;
		Uint8 bestIdx = 0;
		for (int c = 0; c < dn; ++c) {
			int dr = dc[c].r - sr;
			int dg = dc[c].g - sg;
			int db = dc[c].b - sb;
			// Redmean approximation for perceptual color distance.
			// Weights red and blue channels based on the average red
			// value of the two colors being compared.  This better
			// preserves hue than plain Euclidean RGB distance.
			int rmean = (sr + dc[c].r) / 2;
			int dist = ((512 + rmean) * dr * dr >> 8) + 4 * dg * dg + ((767 - rmean) * db * db >> 8);
			if (dist < bestDist) {
				bestDist = dist;
				bestIdx = static_cast<Uint8>(c);
				if (dist == 0) {
					break;
				}
			}
		}
		remap[i] = bestIdx;
	}
}

// Apply a remap table to every pixel in an INDEX8 surface (in-place).
static void RemapSurfacePixels(SDL_Surface* surf, const Uint8* remap)
{
	Uint8* px = static_cast<Uint8*>(surf->pixels);
	int pitch = surf->pitch;
	for (int y = 0; y < surf->h; ++y) {
		Uint8* row = px + y * pitch;
		for (int x = 0; x < surf->w; ++x) {
			row[x] = remap[row[x]];
		}
	}
}

// Remap an already-duplicated surface's pixels from its own palette to the
// given target palette.  Called from BeginFrame so the remap always uses the
// palette that will be active for this frame's Flip.
static void RemapSurfaceToTargetPalette(SDL_Surface* surf, SDL_Palette* targetPal)
{
	SDL_Palette* srcPal = SDL_GetSurfacePalette(surf);
	if (!srcPal || !targetPal || srcPal == targetPal) {
		return;
	}

	Uint8 remap[256];
	BuildPaletteRemap(remap, srcPal, targetPal);

	bool wasLocked = (surf->flags & SDL_SURFACE_LOCKED) != 0;
	if (!wasLocked) {
		SDL_LockSurface(surf);
	}
	RemapSurfacePixels(surf, remap);
	if (!wasLocked) {
		SDL_UnlockSurface(surf);
	}

	SDL_SetSurfacePalette(surf, targetPal);
}

Uint32 Direct3DRMPaletteSWRenderer::GetTextureId(IDirect3DRMTexture* iTexture, bool isUI, float scaleX, float scaleY)
{
	auto texture = static_cast<Direct3DRMTextureImpl*>(iTexture);
	auto surface = static_cast<DirectDrawSurfaceImpl*>(texture->m_surface);

	// Check if already mapped
	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& texRef = m_textures[i];
		if (texRef.texture == texture) {
			if (isUI) {
				// UI textures: always use the original surface directly.
				// The game modifies these in-place (e.g. mosaic transition),
				// so a cached duplicate would be stale.
				texRef.cached = surface->m_surface;
			}
			else if (texRef.version != texture->m_version || !texRef.cached) {
				if (texRef.cached) {
					SDL_DestroySurface(texRef.cached);
				}
				// 3D textures: duplicate and remap to the flip palette.
				texRef.cached = SDL_DuplicateSurface(surface->m_surface);
				SDL_LockSurface(texRef.cached);
				if (m_flipPalette) {
					RemapSurfaceToTargetPalette(texRef.cached, m_flipPalette);
				}
				texRef.version = texture->m_version;
			}
			return i;
		}
	}

	SDL_Surface* converted;
	if (isUI) {
		// Use the original surface directly — no duplicate.
		converted = surface->m_surface;
	}
	else {
		// 3D textures: duplicate and remap to the flip palette.
		converted = SDL_DuplicateSurface(surface->m_surface);
		SDL_LockSurface(converted);
		if (m_flipPalette) {
			RemapSurfaceToTargetPalette(converted, m_flipPalette);
		}
	}

	// Reuse freed slot
	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& texRef = m_textures[i];
		if (!texRef.texture) {
			texRef = {texture, texture->m_version, converted};
			AddTextureDestroyCallback(i, texture);
			return i;
		}
	}

	m_textures.push_back({texture, texture->m_version, converted});
	AddTextureDestroyCallback(static_cast<Uint32>(m_textures.size() - 1), texture);
	return static_cast<Uint32>(m_textures.size() - 1);
}

static PaletteMeshCache PalUploadMesh(const MeshGroup& meshGroup)
{
	PaletteMeshCache cache{&meshGroup, meshGroup.version};
	cache.flat = meshGroup.quality == D3DRMRENDER_FLAT || meshGroup.quality == D3DRMRENDER_UNLITFLAT;

	if (cache.flat) {
		FlattenSurfaces(
			meshGroup.vertices.data(),
			meshGroup.vertices.size(),
			meshGroup.indices.data(),
			meshGroup.indices.size(),
			meshGroup.texture != nullptr,
			cache.vertices,
			cache.indices
		);
	}
	else {
		cache.vertices.assign(meshGroup.vertices.begin(), meshGroup.vertices.end());
		cache.indices.assign(meshGroup.indices.begin(), meshGroup.indices.end());
	}

	return cache;
}

void Direct3DRMPaletteSWRenderer::AddMeshDestroyCallback(Uint32 id, IDirect3DRMMesh* mesh)
{
	auto* ctx = new PalCacheDestroyContext{this, id};
	mesh->AddDestroyCallback(
		[](IDirect3DRMObject* obj, void* arg) {
			auto* ctx = static_cast<PalCacheDestroyContext*>(arg);
			auto& cacheEntry = ctx->renderer->m_meshes[ctx->id];
			if (cacheEntry.meshGroup) {
				cacheEntry.meshGroup = nullptr;
				cacheEntry.vertices.clear();
				cacheEntry.indices.clear();
			}
			delete ctx;
		},
		ctx
	);
}

Uint32 Direct3DRMPaletteSWRenderer::GetMeshId(IDirect3DRMMesh* mesh, const MeshGroup* meshGroup)
{
	for (Uint32 i = 0; i < m_meshes.size(); ++i) {
		auto& cache = m_meshes[i];
		if (cache.meshGroup == meshGroup) {
			if (cache.version != meshGroup->version) {
				cache = std::move(PalUploadMesh(*meshGroup));
			}
			return i;
		}
	}

	auto newCache = PalUploadMesh(*meshGroup);

	for (Uint32 i = 0; i < m_meshes.size(); ++i) {
		auto& cache = m_meshes[i];
		if (!cache.meshGroup) {
			cache = std::move(newCache);
			AddMeshDestroyCallback(i, mesh);
			return i;
		}
	}

	m_meshes.push_back(std::move(newCache));
	AddMeshDestroyCallback((Uint32) (m_meshes.size() - 1), mesh);
	return (Uint32) (m_meshes.size() - 1);
}

HRESULT Direct3DRMPaletteSWRenderer::BeginFrame()
{
	if (!m_renderedImage || !SDL_LockSurface(m_renderedImage)) {
		return DDERR_GENERIC;
	}

	// Rebuild lighting LUT if palette changed
	if (m_lightLUTDirty) {
		m_palette = SDL_GetSurfacePalette(m_renderedImage);
		BuildLightingLUT();
		BuildBlendLUT();
	}

	// Use the palette snapshot from the previous Flip (m_flipPalette) for
	// texture remapping.  Only remap when the flip palette actually changes
	// (i.e. on scene transitions), not every frame.
	if (m_flipPalette && m_flipPaletteDirty) {
		m_flipPaletteDirty = false;

		int grassGreens = 0;
		for (int i = 0; i < m_flipPalette->ncolors; ++i) {
			SDL_Color c = m_flipPalette->colors[i];
			if (c.g >= 60 && c.g <= 125 && c.r >= 35 && c.r <= 95 && c.b >= 20 && c.b <= 65 && c.g > c.r) {
				grassGreens++;
			}
		}
		int invalidated = 0;

		// Invalidate all cached 3D textures so they get re-remapped
		// against the new palette on next use in GetTextureId.
		for (auto& texRef : m_textures) {
			if (!texRef.texture || !texRef.cached) {
				continue;
			}
			auto* origSurface =
				static_cast<DirectDrawSurfaceImpl*>(static_cast<Direct3DRMTextureImpl*>(texRef.texture)->m_surface);
			if (texRef.cached == origSurface->m_surface) {
				continue;
			}
			SDL_UnlockSurface(texRef.cached);
			SDL_DestroySurface(texRef.cached);
			texRef.cached = nullptr;
			texRef.version = 0;
			invalidated++;
		}

		// Rebuild lighting/blend LUTs for the new palette
		BuildLightingLUT();
		BuildBlendLUT();
	}

	ClearZBuffer();
	m_transparencyEnabled = false;
	return DD_OK;
}

void Direct3DRMPaletteSWRenderer::EnableTransparency()
{
	m_transparencyEnabled = true;
}

void Direct3DRMPaletteSWRenderer::SubmitDraw(
	DWORD meshId,
	const D3DRMMATRIX4D& modelViewMatrix,
	const D3DRMMATRIX4D& worldMatrix,
	const D3DRMMATRIX4D& viewMatrix,
	const Matrix3x3& normalMatrix,
	const Appearance& appearance
)
{
	memcpy(m_normalMatrix, normalMatrix, sizeof(Matrix3x3));

	auto& mesh = m_meshes[meshId];

	// Pre-transform all vertex positions and normals
	m_transformedVerts.clear();
	m_transformedVerts.reserve(mesh.vertices.size());
	for (const auto& src : mesh.vertices) {
		D3DRMVERTEX& dst = m_transformedVerts.emplace_back();
		dst.position = TransformPoint(src.position, modelViewMatrix);
		dst.normal = src.normal;
		dst.texCoord = src.texCoord;
	}

	// Assemble triangles using index buffer
	for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
		DrawTriangleClipped(
			{m_transformedVerts[mesh.indices[i]],
			 m_transformedVerts[mesh.indices[i + 1]],
			 m_transformedVerts[mesh.indices[i + 2]]},
			appearance
		);
	}
}

HRESULT Direct3DRMPaletteSWRenderer::FinalizeFrame()
{
	SDL_UnlockSurface(m_renderedImage);

	return DD_OK;
}

void Direct3DRMPaletteSWRenderer::Resize(int width, int height, const ViewportTransform& viewportTransform)
{
	m_viewportTransform = viewportTransform;
	float aspect = static_cast<float>(width) / height;
	float virtualAspect = static_cast<float>(m_virtualWidth) / m_virtualHeight;

	// Cap to virtual canvase for performance
	if (aspect > virtualAspect) {
		m_height = std::min(height, (int) m_virtualHeight);
		m_width = static_cast<int>(m_height * aspect);
	}
	else {
		m_width = std::min(width, (int) m_virtualWidth);
		m_height = static_cast<int>(m_width / aspect);
	}

	m_viewportTransform.scale =
		std::min(static_cast<float>(m_width) / m_virtualWidth, static_cast<float>(m_height) / m_virtualHeight);

	m_viewportTransform.offsetX = (m_width - (m_virtualWidth * m_viewportTransform.scale)) / 2.0f;
	m_viewportTransform.offsetY = (m_height - (m_virtualHeight * m_viewportTransform.scale)) / 2.0f;

	if (m_renderedImage) {
		SDL_DestroySurface(m_renderedImage);
	}
	m_renderedImage = SDL_CreateSurface(m_width, m_height, SDL_PIXELFORMAT_INDEX8);

	// If we already have a palette, attach it to the new surface
	if (m_palette) {
		SDL_SetSurfacePalette(m_renderedImage, m_palette);
	}

	m_zBuffer.resize(m_width * m_height);
}

void Direct3DRMPaletteSWRenderer::Clear(float r, float g, float b)
{
	if (!m_palette) {
		SDL_FillSurfaceRect(m_renderedImage, nullptr, 0);
		return;
	}

	// Find nearest palette entry
	Uint8 tr = static_cast<Uint8>(r * 255);
	Uint8 tg = static_cast<Uint8>(g * 255);
	Uint8 tb = static_cast<Uint8>(b * 255);
	int bestDist = INT_MAX;
	Uint8 bestIdx = 0;
	for (int c = 0; c < m_palette->ncolors; ++c) {
		int dr = m_palette->colors[c].r - tr;
		int dg = m_palette->colors[c].g - tg;
		int db = m_palette->colors[c].b - tb;
		int dist = dr * dr + dg * dg + db * db;
		if (dist < bestDist) {
			bestDist = dist;
			bestIdx = static_cast<Uint8>(c);
			if (dist == 0) {
				break;
			}
		}
	}

	SDL_FillSurfaceRect(m_renderedImage, nullptr, bestIdx);
}

void Direct3DRMPaletteSWRenderer::Flip()
{
	if (!m_renderedImage || !m_renderedImage->pixels) {
		return;
	}

	SDL_Surface* winSurface = SDL_GetWindowSurface(DDWindow);
	if (!winSurface) {
		return;
	}
	if (!winSurface->pixels) {
		return;
	}

	if (winSurface->format == SDL_PIXELFORMAT_INDEX8) {
		// Window surface is paletted — copy indices directly and set
		// the palette on the destination so the DAC/display picks it up.
		if (m_palette) {
			SDL_SetSurfacePalette(winSurface, m_palette);
		}

		Uint8* src = static_cast<Uint8*>(m_renderedImage->pixels);
		Uint8* dst = static_cast<Uint8*>(winSurface->pixels);
		int srcPitch = m_renderedImage->pitch;
		int dstPitch = winSurface->pitch;

		if (m_width * 2 <= winSurface->w && m_height * 2 <= winSurface->h) {
			// 2x nearest-neighbor upscale (half-res rendering)
			for (int row = 0; row < m_height; ++row) {
				Uint8* srcRow = src + row * srcPitch;
				Uint8* dstRow0 = dst + (row * 2) * dstPitch;
				Uint8* dstRow1 = dstRow0 + dstPitch;
				for (int col = 0; col < m_width; ++col) {
					Uint8 px = srcRow[col];
					dstRow0[col * 2] = px;
					dstRow0[col * 2 + 1] = px;
					dstRow1[col * 2] = px;
					dstRow1[col * 2 + 1] = px;
				}
			}
		}
		else {
			int copyH = std::min(m_height, winSurface->h);
			int copyW = std::min(m_width, winSurface->w);
			if (srcPitch == dstPitch && copyW == m_width) {
				memcpy(dst, src, static_cast<size_t>(srcPitch) * copyH);
			}
			else {
				for (int row = 0; row < copyH; ++row) {
					memcpy(dst + row * dstPitch, src + row * srcPitch, copyW);
				}
			}
		}
	}
	else {
		// Window surface is not paletted — let SDL convert INDEX8 → dest format.
		// Use scaled blit to handle fullscreen on high-res displays.
		if (m_palette) {
			SDL_SetSurfacePalette(m_renderedImage, m_palette);
			SDL_BlitSurfaceScaled(m_renderedImage, nullptr, winSurface, nullptr, SDL_SCALEMODE_NEAREST);
		}
	}

	SDL_UpdateWindowSurface(DDWindow);

	// Snapshot the palette for the lighting LUT.
	SDL_Palette* displayedPal =
		(winSurface->format == SDL_PIXELFORMAT_INDEX8) ? SDL_GetSurfacePalette(winSurface) : m_palette;
	if (displayedPal) {
		if (!m_flipPalette) {
			m_flipPalette = SDL_CreatePalette(256);
		}
		if (!PalettesEqual(displayedPal, m_flipPalette)) {
			SDL_SetPaletteColors(m_flipPalette, displayedPal->colors, 0, displayedPal->ncolors);
			m_flipPaletteDirty = true;
		}
	}
}

void Direct3DRMPaletteSWRenderer::Draw2DImage(
	Uint32 textureId,
	const SDL_Rect& srcRect,
	const SDL_Rect& dstRect,
	FColor color
)
{
	SDL_Rect centeredRect = {
		static_cast<int>(dstRect.x * m_viewportTransform.scale + m_viewportTransform.offsetX),
		static_cast<int>(dstRect.y * m_viewportTransform.scale + m_viewportTransform.offsetY),
		static_cast<int>(dstRect.w * m_viewportTransform.scale),
		static_cast<int>(dstRect.h * m_viewportTransform.scale),
	};

	if (textureId == NO_TEXTURE_ID) {
		// Fill with nearest palette colour
		if (m_palette) {
			Uint8 tr = static_cast<Uint8>(color.r * 255);
			Uint8 tg = static_cast<Uint8>(color.g * 255);
			Uint8 tb = static_cast<Uint8>(color.b * 255);
			int bestDist = INT_MAX;
			Uint8 bestIdx = 0;
			for (int c = 0; c < m_palette->ncolors; ++c) {
				int dr = m_palette->colors[c].r - tr;
				int dg = m_palette->colors[c].g - tg;
				int db = m_palette->colors[c].b - tb;
				int dist = dr * dr + dg * dg + db * db;
				if (dist < bestDist) {
					bestDist = dist;
					bestIdx = static_cast<Uint8>(c);
					if (dist == 0) {
						break;
					}
				}
			}
			SDL_FillSurfaceRect(m_renderedImage, &centeredRect, bestIdx);
		}
		else {
			SDL_FillSurfaceRect(m_renderedImage, &centeredRect, 0);
		}
		return;
	}

	// Raw INDEX8 blit — copy palette indices directly, no SDL palette
	// remapping.  This is the hot path for 2D (video, UI overlays).
	SDL_Surface* surface = m_textures[textureId].cached;

	// Only check the surface color key when the caller explicitly requested it
	// (via DDBLT_KEYSRC / DDBLTFAST_SRCCOLORKEY). Many surfaces have a stale
	// color key set that should not be used for normal blits (e.g. SMK video).
	Uint32 colorKey = 0;
	bool hasColorKey = SDL_SurfaceHasColorKey(surface) && SDL_GetSurfaceColorKey(surface, &colorKey);

	bool wasLocked = (surface->flags & SDL_SURFACE_LOCKED) != 0;
	if (wasLocked) {
		SDL_UnlockSurface(surface);
	}
	Uint8* src = static_cast<Uint8*>(surface->pixels);
	Uint8* dst = static_cast<Uint8*>(m_renderedImage->pixels);
	int srcPitch = surface->pitch;
	int dstPitch = m_renderedImage->pitch;

	int dstX0 = std::max(0, centeredRect.x);
	int dstY0 = std::max(0, centeredRect.y);
	int dstX1 = std::min(m_width, centeredRect.x + centeredRect.w);
	int dstY1 = std::min(m_height, centeredRect.y + centeredRect.h);

	Uint8 ckByte = static_cast<Uint8>(colorKey);

	if (!hasColorKey && centeredRect.w == srcRect.w && centeredRect.h == srcRect.h) {
		// 1:1 opaque copy — fast memcpy per scanline
		int copyW = dstX1 - dstX0;
		int copyH = dstY1 - dstY0;
		if (copyW > 0 && copyH > 0) {
			int srcStartX = srcRect.x + (dstX0 - centeredRect.x);
			int srcStartY = srcRect.y + (dstY0 - centeredRect.y);
			for (int row = 0; row < copyH; ++row) {
				memcpy(dst + (dstY0 + row) * dstPitch + dstX0, src + (srcStartY + row) * srcPitch + srcStartX, copyW);
			}
		}
	}
	else if (centeredRect.w == srcRect.w && centeredRect.h == srcRect.h) {
		// 1:1 copy with color key
		int copyW = dstX1 - dstX0;
		int copyH = dstY1 - dstY0;
		int srcStartX = srcRect.x + (dstX0 - centeredRect.x);
		int srcStartY = srcRect.y + (dstY0 - centeredRect.y);
		for (int row = 0; row < copyH; ++row) {
			Uint8* srcRow = src + (srcStartY + row) * srcPitch + srcStartX;
			Uint8* dstRow = dst + (dstY0 + row) * dstPitch + dstX0;
			for (int col = 0; col < copyW; ++col) {
				Uint8 px = srcRow[col];
				if (px != ckByte) {
					dstRow[col] = px;
				}
			}
		}
	}
	else if (!hasColorKey) {
		// Scaled blit, no color key
		for (int dy = dstY0; dy < dstY1; ++dy) {
			int sy = srcRect.y + (dy - centeredRect.y) * srcRect.h / centeredRect.h;
			Uint8* dstRow = dst + dy * dstPitch;
			Uint8* srcRow = src + sy * srcPitch;
			for (int dx = dstX0; dx < dstX1; ++dx) {
				int sx = srcRect.x + (dx - centeredRect.x) * srcRect.w / centeredRect.w;
				dstRow[dx] = srcRow[sx];
			}
		}
	}
	else {
		// Scaled blit with color key
		for (int dy = dstY0; dy < dstY1; ++dy) {
			int sy = srcRect.y + (dy - centeredRect.y) * srcRect.h / centeredRect.h;
			Uint8* dstRow = dst + dy * dstPitch;
			Uint8* srcRow = src + sy * srcPitch;
			for (int dx = dstX0; dx < dstX1; ++dx) {
				int sx = srcRect.x + (dx - centeredRect.x) * srcRect.w / centeredRect.w;
				Uint8 px = srcRow[sx];
				if (px != ckByte) {
					dstRow[dx] = px;
				}
			}
		}
	}
	if (wasLocked) {
		SDL_LockSurface(surface);
	}
}

void Direct3DRMPaletteSWRenderer::SetDither(bool dither)
{
	(void) dither;
}

void Direct3DRMPaletteSWRenderer::SetPalette(SDL_Palette* palette)
{
	m_palette = palette;
	m_lightLUTDirty = true;
	if (m_renderedImage) {
		SDL_SetSurfacePalette(m_renderedImage, palette);
	}
}

void Direct3DRMPaletteSWRenderer::Download(SDL_Surface* target)
{
	if (!m_renderedImage || !target) {
		return;
	}

	// Extract the viewport region (excluding pillarbox/letterbox borders)
	// and scale it to fill the target, matching the software renderer.
	SDL_Rect srcRect = {
		static_cast<int>(m_viewportTransform.offsetX),
		static_cast<int>(m_viewportTransform.offsetY),
		static_cast<int>(m_virtualWidth * m_viewportTransform.scale),
		static_cast<int>(m_virtualHeight * m_viewportTransform.scale),
	};

	if (m_palette) {
		SDL_SetSurfacePalette(m_renderedImage, m_palette);
	}
	SDL_BlitSurfaceScaled(m_renderedImage, &srcRect, target, nullptr, SDL_SCALEMODE_NEAREST);
}
