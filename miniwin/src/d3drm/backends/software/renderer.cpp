#include "d3drmrenderer.h"
#include "d3drmrenderer_software.h"
#include "ddsurface_impl.h"
#include "mathutils.h"
#include "meshutils.h"
#include "miniwin.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

// Perspective-correct UVs are computed exactly every PERSP_STEP pixels and
// interpolated affinely in between.  8 pixels keeps the error well below a
// texel at the resolutions this renderer runs at.
static constexpr int PERSP_STEP = 8;

// Reciprocals for perspective-block lengths 0..PERSP_STEP, so the affine
// step inside a block needs no division.
static const float SW_BLOCK_INV[PERSP_STEP + 1] = {
	0.0f,
	1.0f / 1.0f,
	1.0f / 2.0f,
	1.0f / 3.0f,
	1.0f / 4.0f,
	1.0f / 5.0f,
	1.0f / 6.0f,
	1.0f / 7.0f,
	1.0f / 8.0f,
};

Direct3DRMSoftwareRenderer::Direct3DRMSoftwareRenderer(DWORD width, DWORD height)
{
	m_virtualWidth = width;
	m_virtualHeight = height;

	m_renderer = SDL_CreateRenderer(DDWindow, NULL);

	ViewportTransform viewportTransform = {1.0f, 0.0f, 0.0f};
	Resize(width, height, viewportTransform);
}

Direct3DRMSoftwareRenderer::~Direct3DRMSoftwareRenderer()
{
	SDL_DestroySurface(m_renderedImage);
	SDL_DestroyTexture(m_uploadBuffer);
	SDL_DestroyRenderer(m_renderer);
}

void Direct3DRMSoftwareRenderer::PushLights(const SceneLight* lights, size_t count)
{
	// Fold ambient lights into a single base color and pre-normalize the
	// directional light vectors so per-vertex lighting does no sqrt for them.
	m_preparedLights.clear();
	m_ambientR = 0.0f;
	m_ambientG = 0.0f;
	m_ambientB = 0.0f;

	for (size_t i = 0; i < count; ++i) {
		const SceneLight& light = lights[i];

		if (light.positional == 0.0f && light.directional == 0.0f) {
			m_ambientR += light.color.r;
			m_ambientG += light.color.g;
			m_ambientB += light.color.b;
			continue;
		}

		PreparedLight prepared;
		prepared.color = light.color;
		if (light.directional == 1.0f) {
			prepared.positional = false;
			prepared.vec = Normalize({-light.direction.x, -light.direction.y, -light.direction.z});
		}
		else {
			prepared.positional = true;
			prepared.vec = light.position;
		}
		m_preparedLights.push_back(prepared);
	}
}

void Direct3DRMSoftwareRenderer::SetFrustumPlanes(const Plane* frustumPlanes)
{
	memcpy(m_frustumPlanes, frustumPlanes, sizeof(m_frustumPlanes));
}

void Direct3DRMSoftwareRenderer::SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back)
{
	m_front = front;
	m_back = back;
	memcpy(m_projection, projection, sizeof(D3DRMMATRIX4D));
}

void Direct3DRMSoftwareRenderer::ClearZBuffer()
{
	// 0x7F7F7F7F interpreted as a float is ~3.4e38 — far beyond any depth
	// value we produce, so a byte-pattern memset (vectorized in libc on
	// every platform) is a valid "infinitely far" fill.
	memset(m_zBuffer.data(), 0x7F, m_zBuffer.size() * sizeof(float));
}

void Direct3DRMSoftwareRenderer::ProjectVertex(const D3DVECTOR& v, D3DRMVECTOR4D& p) const
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

static SWLitVertex SplitEdge(SWLitVertex a, const SWLitVertex& b, float plane)
{
	float t = (plane - a.position.z) / (b.position.z - a.position.z);
	a.position.x += t * (b.position.x - a.position.x);
	a.position.y += t * (b.position.y - a.position.y);
	a.position.z = plane;

	a.texCoord.u += t * (b.texCoord.u - a.texCoord.u);
	a.texCoord.v += t * (b.texCoord.v - a.texCoord.v);

	a.color.r = static_cast<Uint8>(a.color.r + t * (b.color.r - a.color.r));
	a.color.g = static_cast<Uint8>(a.color.g + t * (b.color.g - a.color.g));
	a.color.b = static_cast<Uint8>(a.color.b + t * (b.color.b - a.color.b));
	a.color.a = static_cast<Uint8>(a.color.a + t * (b.color.a - a.color.a));

	return a;
}

static bool IsTriangleOutsideViewCone(
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

void Direct3DRMSoftwareRenderer::DrawTriangleClipped(const SWLitVertex (&v)[3], const Appearance& appearance)
{
	bool in0 = v[0].position.z >= m_front;
	bool in1 = v[1].position.z >= m_front;
	bool in2 = v[2].position.z >= m_front;

	int insideCount = in0 + in1 + in2;

	if (insideCount == 0 || v[0].position.z > m_back && v[1].position.z > m_back && v[2].position.z > m_back) {
		return; // Outside clipping
	}
	if (IsTriangleOutsideViewCone(v[0].position, v[1].position, v[2].position, m_frustumPlanes)) {
		return;
	}

	if (insideCount == 3) {
		DrawTriangleProjected(v[0], v[1], v[2], appearance);
	}
	else if (insideCount == 2) {
		SWLitVertex split;
		if (!in0) {
			split = SplitEdge(v[2], v[0], m_front);
			DrawTriangleProjected(v[1], v[2], split, appearance);
			DrawTriangleProjected(v[1], split, SplitEdge(v[1], v[0], m_front), appearance);
		}
		else if (!in1) {
			split = SplitEdge(v[0], v[1], m_front);
			DrawTriangleProjected(v[2], v[0], split, appearance);
			DrawTriangleProjected(v[2], split, SplitEdge(v[2], v[1], m_front), appearance);
		}
		else {
			split = SplitEdge(v[1], v[2], m_front);
			DrawTriangleProjected(v[0], v[1], split, appearance);
			DrawTriangleProjected(v[0], split, SplitEdge(v[0], v[2], m_front), appearance);
		}
	}
	else if (in0) {
		DrawTriangleProjected(v[0], SplitEdge(v[0], v[1], m_front), SplitEdge(v[0], v[2], m_front), appearance);
	}
	else if (in1) {
		DrawTriangleProjected(SplitEdge(v[1], v[0], m_front), v[1], SplitEdge(v[1], v[2], m_front), appearance);
	}
	else {
		DrawTriangleProjected(SplitEdge(v[2], v[0], m_front), SplitEdge(v[2], v[1], m_front), v[2], appearance);
	}
}

// The render surface and all cached textures are SDL_PIXELFORMAT_RGBA32
// (bytes R,G,B,A in memory regardless of endianness), so pixels are accessed
// directly instead of through SDL_GetRGBA/SDL_MapRGBA calls per pixel.
inline static void BlendRGBA(Uint8* p, int r, int g, int b, int a)
{
	int inv = 255 - a;
	p[0] = static_cast<Uint8>((r * a + p[0] * inv + 127) / 255);
	p[1] = static_cast<Uint8>((g * a + p[1] * inv + 127) / 255);
	p[2] = static_cast<Uint8>((b * a + p[2] * inv + 127) / 255);
	int outA = a + (p[3] * inv + 127) / 255;
	p[3] = static_cast<Uint8>(outA > 255 ? 255 : outA);
}

SDL_Color Direct3DRMSoftwareRenderer::ApplyLighting(
	const D3DVECTOR& position,
	const D3DVECTOR& oNormal,
	const Appearance& appearance
)
{
	FColor specular = {0, 0, 0, 0};
	FColor diffuse = {m_ambientR, m_ambientG, m_ambientB, 0};

	D3DVECTOR normal = Normalize(TransformNormal(oNormal, m_normalMatrix));

	bool haveViewVec = false;
	D3DVECTOR viewVec;

	for (const auto& light : m_preparedLights) {
		D3DVECTOR lightVec;
		if (light.positional) {
			lightVec = Normalize({light.vec.x - position.x, light.vec.y - position.y, light.vec.z - position.z});
		}
		else {
			lightVec = light.vec;
		}

		float dotNL = DotProduct(normal, lightVec);
		if (dotNL > 0.0f) {
			// Diffuse contribution
			diffuse.r += dotNL * light.color.r;
			diffuse.g += dotNL * light.color.g;
			diffuse.b += dotNL * light.color.b;

			// Specular
			if (appearance.shininess > 0.0f && !light.positional) {
				if (!haveViewVec) {
					viewVec = Normalize({-position.x, -position.y, -position.z});
					haveViewVec = true;
				}
				D3DVECTOR H = Normalize({lightVec.x + viewVec.x, lightVec.y + viewVec.y, lightVec.z + viewVec.z});

				float dotNH = std::max(DotProduct(normal, H), 0.0f);
				float spec = std::pow(dotNH, appearance.shininess);

				specular.r += spec * light.color.r;
				specular.g += spec * light.color.g;
				specular.b += spec * light.color.b;
			}
		}
	}

	return SDL_Color{
		static_cast<Uint8>(std::min(255.0f, diffuse.r * appearance.color.r + specular.r * 255.0f)),
		static_cast<Uint8>(std::min(255.0f, diffuse.g * appearance.color.g + specular.g * 255.0f)),
		static_cast<Uint8>(std::min(255.0f, diffuse.b * appearance.color.b + specular.b * 255.0f)),
		appearance.color.a
	};
}

inline D3DVECTOR Subtract(const D3DVECTOR& a, const D3DVECTOR& b)
{
	return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline bool IsBackface(const D3DVECTOR& v0, const D3DVECTOR& v1, const D3DVECTOR& v2)
{
	D3DVECTOR normal = CrossProduct(Subtract(v1, v0), Subtract(v2, v0));

	return DotProduct(normal, v0) >= 0.0f;
}

// Rasterizer vertex: screen position, vertex color and perspective UVs.
struct SWVertexXY {
	float x, y, z;
	float r, g, b;
	float u_over_w, v_over_w, one_over_w;
};

// Interpolated values along a triangle edge, stepped once per scanline.
struct SWEdge {
	float x, z, r, g, b, uw, vw, ow;
};

inline static void SWSetupEdge(const SWVertexXY& a, const SWVertexXY& b, int startY, SWEdge& e, SWEdge& step)
{
	float dy = b.y - a.y;
	float invDy = (dy != 0.0f) ? 1.0f / dy : 0.0f;
	float t0 = (startY - a.y) * invDy;
	e.x = a.x + t0 * (b.x - a.x);
	e.z = a.z + t0 * (b.z - a.z);
	e.r = a.r + t0 * (b.r - a.r);
	e.g = a.g + t0 * (b.g - a.g);
	e.b = a.b + t0 * (b.b - a.b);
	e.uw = a.u_over_w + t0 * (b.u_over_w - a.u_over_w);
	e.vw = a.v_over_w + t0 * (b.v_over_w - a.v_over_w);
	e.ow = a.one_over_w + t0 * (b.one_over_w - a.one_over_w);
	step.x = (b.x - a.x) * invDy;
	step.z = (b.z - a.z) * invDy;
	step.r = (b.r - a.r) * invDy;
	step.g = (b.g - a.g) * invDy;
	step.b = (b.b - a.b) * invDy;
	step.uw = (b.u_over_w - a.u_over_w) * invDy;
	step.vw = (b.v_over_w - a.v_over_w) * invDy;
	step.ow = (b.one_over_w - a.one_over_w) * invDy;
}

inline static void SWStepEdge(SWEdge& e, const SWEdge& s)
{
	e.x += s.x;
	e.z += s.z;
	e.r += s.r;
	e.g += s.g;
	e.b += s.b;
	e.uw += s.uw;
	e.vw += s.vw;
	e.ow += s.ow;
}

inline static float SWClampChannel(float c)
{
	return c < 0.0f ? 0.0f : (c > 255.0f ? 255.0f : c);
}

void Direct3DRMSoftwareRenderer::DrawTriangleProjected(
	const SWLitVertex& v0,
	const SWLitVertex& v1,
	const SWLitVertex& v2,
	const Appearance& appearance
)
{
	const Uint8 triAlpha = appearance.color.a;

	Uint32 textureId = appearance.textureId;
	int texturePitch = 0;
	Uint8* texels = nullptr;
	int texWidthScale = 0;
	int texHeightScale = 0;
	// Power-of-two textures (the normal case) are sampled with integer
	// 16.16 coordinates and shift+mask wrapping.
	bool fastTex = false;
	int uMask = 0, vMask = 0, pitchShift = 0;
	float texWFix = 0.0f, texHFix = 0.0f;

	if (textureId != NO_TEXTURE_ID) {
		SDL_Surface* texture = m_textures[textureId].cached;
		if (texture) {
			texturePitch = texture->pitch;
			texels = static_cast<Uint8*>(texture->pixels);
			int tw = texture->w;
			int th = texture->h;
			texWidthScale = tw - 1;
			texHeightScale = th - 1;
			if ((tw & (tw - 1)) == 0 && (th & (th - 1)) == 0 && (texturePitch & (texturePitch - 1)) == 0) {
				fastTex = true;
				uMask = tw - 1;
				vMask = th - 1;
				while ((1 << pitchShift) < texturePitch) {
					++pitchShift;
				}
				texWFix = static_cast<float>(tw) * 65536.0f;
				texHFix = static_cast<float>(th) * 65536.0f;
			}
		}
	}

	if (!texels && triAlpha == 0) {
		// Fully transparent material — nothing to draw.
		return;
	}

	D3DRMVECTOR4D p0, p1, p2;
	ProjectVertex(v0.position, p0);
	ProjectVertex(v1.position, p1);
	ProjectVertex(v2.position, p2);

	SWVertexXY verts[3] = {
		{p0.x, p0.y, p0.z, (float) v0.color.r, (float) v0.color.g, (float) v0.color.b, 0, 0, 0},
		{p1.x, p1.y, p1.z, (float) v1.color.r, (float) v1.color.g, (float) v1.color.b, 0, 0, 0},
		{p2.x, p2.y, p2.z, (float) v2.color.r, (float) v2.color.g, (float) v2.color.b, 0, 0, 0},
	};

	if (texels) {
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

	int minY = std::max(0, (int) std::ceil(verts[0].y));
	int maxY = std::min((int) m_height - 1, (int) std::floor(verts[2].y));
	if (minY > maxY) {
		return;
	}

	Uint8* pixels = (Uint8*) m_renderedImage->pixels;
	int pitch = m_renderedImage->pitch;

	// Incremental edge walking: interpolate along the long edge (0->2) and
	// the two short segments (0->1, 1->2), stepping once per scanline
	// instead of re-interpolating every attribute per pixel.
	SWEdge longEdge, longStep;
	SWSetupEdge(verts[0], verts[2], minY, longEdge, longStep);

	SWEdge shortEdge, shortStep;
	int midY = static_cast<int>(std::ceil(verts[1].y));
	bool pastMid = (minY >= midY);
	if (pastMid) {
		SWSetupEdge(verts[1], verts[2], minY, shortEdge, shortStep);
	}
	else {
		SWSetupEdge(verts[0], verts[1], minY, shortEdge, shortStep);
	}

	for (int y = minY; y <= maxY; ++y) {
		if (!pastMid && y >= midY) {
			pastMid = true;
			SWSetupEdge(verts[1], verts[2], y, shortEdge, shortStep);
		}

		const SWEdge& left = (shortEdge.x <= longEdge.x) ? shortEdge : longEdge;
		const SWEdge& right = (shortEdge.x <= longEdge.x) ? longEdge : shortEdge;

		int startX = std::max(0, (int) std::ceil(left.x));
		int endX = std::min((int) m_width - 1, (int) std::floor(right.x));

		float span = right.x - left.x;
		if (span <= 0.0f || startX > endX) {
			SWStepEdge(shortEdge, shortStep);
			SWStepEdge(longEdge, longStep);
			continue;
		}

		float invSpan = 1.0f / span;
		float startT = (startX - left.x) * invSpan;

		float z = left.z + startT * (right.z - left.z);
		float zStep = (right.z - left.z) * invSpan;

		// Clamp colors at the span endpoints; interpolated values stay
		// inside, so the pixel loops need no per-pixel clamp.
		float lr = SWClampChannel(left.r), lg = SWClampChannel(left.g), lb = SWClampChannel(left.b);
		float rr = SWClampChannel(right.r), rg = SWClampChannel(right.g), rb = SWClampChannel(right.b);

		// Vertex colors in 8.8 fixed point, stepped with integer adds.
		int rFix = static_cast<int>((lr + startT * (rr - lr)) * 256.0f);
		int gFix = static_cast<int>((lg + startT * (rg - lg)) * 256.0f);
		int bFix = static_cast<int>((lb + startT * (rb - lb)) * 256.0f);
		int rStep = static_cast<int>((rr - lr) * invSpan * 256.0f);
		int gStep = static_cast<int>((rg - lg) * invSpan * 256.0f);
		int bStep = static_cast<int>((rb - lb) * invSpan * 256.0f);

		Uint8* row = pixels + y * pitch;
		float* zPtr = &m_zBuffer[y * m_width + startX];

		if (texels) {
			float uow = left.uw + startT * (right.uw - left.uw);
			float vow = left.vw + startT * (right.vw - left.vw);
			float oow = left.ow + startT * (right.ow - left.ow);
			float uowStep = (right.uw - left.uw) * invSpan;
			float vowStep = (right.vw - left.vw) * invSpan;
			float oowStep = (right.ow - left.ow) * invSpan;

			// Perspective-correct UV at the span start; carried block to
			// block so each block costs a single divide.
			float invW = 1.0f / oow;
			float u0 = uow * invW;
			float vv0 = vow * invW;

			int x = startX;
			while (x <= endX) {
				int remaining = endX - x + 1;
				int blockLen = (remaining > PERSP_STEP) ? PERSP_STEP : remaining;

				uow += uowStep * blockLen;
				vow += vowStep * blockLen;
				oow += oowStep * blockLen;
				invW = 1.0f / oow;
				float u1 = uow * invW;
				float vv1 = vow * invW;

				float invBlock = SW_BLOCK_INV[blockLen];

				if (fastTex) {
					Sint32 uFix = static_cast<Sint32>(u0 * texWFix);
					Sint32 vFix = static_cast<Sint32>(vv0 * texHFix);
					Sint32 uStepFix = static_cast<Sint32>((u1 - u0) * invBlock * texWFix);
					Sint32 vStepFix = static_cast<Sint32>((vv1 - vv0) * invBlock * texHFix);

					for (int i = 0; i < blockLen; ++i, ++x) {
						if (z < *zPtr) {
							const Uint8* t =
								texels + ((((vFix >> 16) & vMask) << pitchShift) | (((uFix >> 16) & uMask) << 2));
							int ta = t[3];
							if (ta != 0) {
								int cr = ((rFix >> 8) * t[0] + 127) / 255;
								int cg = ((gFix >> 8) * t[1] + 127) / 255;
								int cb = ((bFix >> 8) * t[2] + 127) / 255;
								Uint8* p = row + x * 4;
								if (ta == 255) {
									*zPtr = z;
									p[0] = static_cast<Uint8>(cr);
									p[1] = static_cast<Uint8>(cg);
									p[2] = static_cast<Uint8>(cb);
									p[3] = 255;
								}
								else {
									BlendRGBA(p, cr, cg, cb, ta);
								}
							}
						}
						z += zStep;
						rFix += rStep;
						gFix += gStep;
						bFix += bStep;
						uFix += uStepFix;
						vFix += vStepFix;
						++zPtr;
					}
				}
				else {
					// Non-power-of-two texture fallback: affine float UVs
					float uAffStep = (u1 - u0) * invBlock;
					float vAffStep = (vv1 - vv0) * invBlock;
					float uAff = u0;
					float vAff = vv0;

					for (int i = 0; i < blockLen; ++i, ++x) {
						if (z < *zPtr) {
							float u = uAff - std::floor(uAff);
							float v = vAff - std::floor(vAff);

							int texX = static_cast<int>(u * texWidthScale);
							int texY = static_cast<int>(v * texHeightScale);

							const Uint8* t = texels + texY * texturePitch + texX * 4;
							int ta = t[3];
							if (ta != 0) {
								int cr = ((rFix >> 8) * t[0] + 127) / 255;
								int cg = ((gFix >> 8) * t[1] + 127) / 255;
								int cb = ((bFix >> 8) * t[2] + 127) / 255;
								Uint8* p = row + x * 4;
								if (ta == 255) {
									*zPtr = z;
									p[0] = static_cast<Uint8>(cr);
									p[1] = static_cast<Uint8>(cg);
									p[2] = static_cast<Uint8>(cb);
									p[3] = 255;
								}
								else {
									BlendRGBA(p, cr, cg, cb, ta);
								}
							}
						}
						z += zStep;
						rFix += rStep;
						gFix += gStep;
						bFix += bStep;
						uAff += uAffStep;
						vAff += vAffStep;
						++zPtr;
					}
				}

				u0 = u1;
				vv0 = vv1;
			}
		}
		else if (triAlpha == 255) {
			// Opaque untextured span
			for (int x = startX; x <= endX; ++x, ++zPtr, z += zStep, rFix += rStep, gFix += gStep, bFix += bStep) {
				if (z >= *zPtr) {
					continue;
				}
				*zPtr = z;
				Uint8* p = row + x * 4;
				p[0] = static_cast<Uint8>(rFix >> 8);
				p[1] = static_cast<Uint8>(gFix >> 8);
				p[2] = static_cast<Uint8>(bFix >> 8);
				p[3] = 255;
			}
		}
		else {
			// Alpha-blended untextured span (does not write depth)
			for (int x = startX; x <= endX; ++x, ++zPtr, z += zStep, rFix += rStep, gFix += gStep, bFix += bStep) {
				if (z >= *zPtr) {
					continue;
				}
				BlendRGBA(row + x * 4, rFix >> 8, gFix >> 8, bFix >> 8, triAlpha);
			}
		}

		SWStepEdge(shortEdge, shortStep);
		SWStepEdge(longEdge, longStep);
	}
}

struct CacheDestroyContext {
	Direct3DRMSoftwareRenderer* renderer;
	Uint32 id;
};

void Direct3DRMSoftwareRenderer::AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture)
{
	auto* ctx = new CacheDestroyContext{this, id};
	texture->AddDestroyCallback(
		[](IDirect3DRMObject* obj, void* arg) {
			auto* ctx = static_cast<CacheDestroyContext*>(arg);
			auto& cacheEntry = ctx->renderer->m_textures[ctx->id];
			if (cacheEntry.cached) {
				SDL_UnlockSurface(cacheEntry.cached);
				SDL_DestroySurface(cacheEntry.cached);
				cacheEntry.cached = nullptr;
				cacheEntry.texture = nullptr;
			}
			delete ctx;
		},
		ctx
	);
}

Uint32 Direct3DRMSoftwareRenderer::GetTextureId(IDirect3DRMTexture* iTexture, bool isUI, float scaleX, float scaleY)
{
	auto texture = static_cast<Direct3DRMTextureImpl*>(iTexture);
	auto surface = static_cast<DirectDrawSurfaceImpl*>(texture->m_surface);

	// Check if already mapped
	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& texRef = m_textures[i];
		if (texRef.texture == texture) {
			if (texRef.version != texture->m_version) {
				// Update animated textures
				SDL_DestroySurface(texRef.cached);
				texRef.cached = SDL_ConvertSurface(surface->m_surface, m_renderedImage->format);
				SDL_LockSurface(texRef.cached);
				texRef.version = texture->m_version;
			}
			return i;
		}
	}

	SDL_Surface* convertedRender = SDL_ConvertSurface(surface->m_surface, m_renderedImage->format);
	SDL_LockSurface(convertedRender);

	// Reuse freed slot
	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& texRef = m_textures[i];
		if (!texRef.texture) {
			texRef = {texture, texture->m_version, convertedRender};
			AddTextureDestroyCallback(i, texture);
			return i;
		}
	}

	// Append new
	m_textures.push_back({texture, texture->m_version, convertedRender});
	AddTextureDestroyCallback(static_cast<Uint32>(m_textures.size() - 1), texture);
	return static_cast<Uint32>(m_textures.size() - 1);
}

MeshCache UploadMesh(const MeshGroup& meshGroup)
{
	MeshCache cache{&meshGroup, meshGroup.version};

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

void Direct3DRMSoftwareRenderer::AddMeshDestroyCallback(Uint32 id, IDirect3DRMMesh* mesh)
{
	auto* ctx = new CacheDestroyContext{this, id};
	mesh->AddDestroyCallback(
		[](IDirect3DRMObject* obj, void* arg) {
			auto* ctx = static_cast<CacheDestroyContext*>(arg);
			auto& cacheEntry = ctx->renderer->m_meshs[ctx->id];
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

Uint32 Direct3DRMSoftwareRenderer::GetMeshId(IDirect3DRMMesh* mesh, const MeshGroup* meshGroup)
{
	for (Uint32 i = 0; i < m_meshs.size(); ++i) {
		auto& cache = m_meshs[i];
		if (cache.meshGroup == meshGroup) {
			if (cache.version != meshGroup->version) {
				cache = std::move(UploadMesh(*meshGroup));
			}
			return i;
		}
	}

	auto newCache = UploadMesh(*meshGroup);

	for (Uint32 i = 0; i < m_meshs.size(); ++i) {
		auto& cache = m_meshs[i];
		if (!cache.meshGroup) {
			cache = std::move(newCache);
			AddMeshDestroyCallback(i, mesh);
			return i;
		}
	}

	m_meshs.push_back(std::move(newCache));
	AddMeshDestroyCallback((Uint32) (m_meshs.size() - 1), mesh);
	return (Uint32) (m_meshs.size() - 1);
}

HRESULT Direct3DRMSoftwareRenderer::BeginFrame()
{
	if (!m_renderedImage || !SDL_LockSurface(m_renderedImage)) {
		return DDERR_GENERIC;
	}
	ClearZBuffer();

	m_format = SDL_GetPixelFormatDetails(m_renderedImage->format);
	m_palette = SDL_GetSurfacePalette(m_renderedImage);

	return DD_OK;
}

void Direct3DRMSoftwareRenderer::EnableTransparency()
{
}

void Direct3DRMSoftwareRenderer::SubmitDraw(
	DWORD meshId,
	const D3DRMMATRIX4D& modelViewMatrix,
	const D3DRMMATRIX4D& worldMatrix,
	const D3DRMMATRIX4D& viewMatrix,
	const Matrix3x3& normalMatrix,
	const Appearance& appearance
)
{
	memcpy(m_normalMatrix, normalMatrix, sizeof(Matrix3x3));

	auto& mesh = m_meshs[meshId];
	const size_t vertexCount = mesh.vertices.size();

	// Pre-transform all vertex positions
	m_transformedPositions.resize(vertexCount);
	for (size_t i = 0; i < vertexCount; ++i) {
		m_transformedPositions[i] = TransformPoint(mesh.vertices[i].position, modelViewMatrix);
	}

	// Lighting is computed lazily, once per unique vertex, and only for
	// vertices of triangles that survive backface culling.  Shared vertices
	// previously got re-lit for every triangle that used them.
	m_vertexColors.resize(vertexCount);
	m_vertexLit.assign(vertexCount, 0);

	const bool flat = appearance.flat;

	// Assemble triangles using index buffer
	for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
		const uint16_t i0 = mesh.indices[i];
		const uint16_t i1 = mesh.indices[i + 1];
		const uint16_t i2 = mesh.indices[i + 2];
		const D3DVECTOR& p0 = m_transformedPositions[i0];
		const D3DVECTOR& p1 = m_transformedPositions[i1];
		const D3DVECTOR& p2 = m_transformedPositions[i2];

		// Cull before lighting and clipping; sub-triangles produced by the
		// near-plane clip are coplanar with the original, so this test is
		// equivalent to the old per-projected-triangle one.
		if (IsBackface(p0, p1, p2)) {
			continue;
		}

		if (!m_vertexLit[i0]) {
			m_vertexLit[i0] = 1;
			m_vertexColors[i0] = ApplyLighting(p0, mesh.vertices[i0].normal, appearance);
		}
		SDL_Color c0 = m_vertexColors[i0];
		SDL_Color c1, c2;
		if (flat) {
			c1 = c0;
			c2 = c0;
		}
		else {
			if (!m_vertexLit[i1]) {
				m_vertexLit[i1] = 1;
				m_vertexColors[i1] = ApplyLighting(p1, mesh.vertices[i1].normal, appearance);
			}
			if (!m_vertexLit[i2]) {
				m_vertexLit[i2] = 1;
				m_vertexColors[i2] = ApplyLighting(p2, mesh.vertices[i2].normal, appearance);
			}
			c1 = m_vertexColors[i1];
			c2 = m_vertexColors[i2];
		}

		SWLitVertex tri[3] = {
			{p0, mesh.vertices[i0].texCoord, c0},
			{p1, mesh.vertices[i1].texCoord, c1},
			{p2, mesh.vertices[i2].texCoord, c2},
		};
		DrawTriangleClipped(tri, appearance);
	}
}

HRESULT Direct3DRMSoftwareRenderer::FinalizeFrame()
{
	SDL_UnlockSurface(m_renderedImage);

	return DD_OK;
}

void Direct3DRMSoftwareRenderer::Resize(int width, int height, const ViewportTransform& viewportTransform)
{
	m_viewportTransform = viewportTransform;
	float aspect = static_cast<float>(width) / height;
	float virtualAspect = static_cast<float>(m_virtualWidth) / m_virtualHeight;

	// Cap to virtual canvase for performance
	if (aspect > virtualAspect) {
		m_height = std::min(height, m_virtualHeight);
		m_width = static_cast<int>(m_height * aspect);
	}
	else {
		m_width = std::min(width, m_virtualWidth);
		m_height = static_cast<int>(m_width / aspect);
	}

	m_viewportTransform.scale =
		std::min(static_cast<float>(m_width) / m_virtualWidth, static_cast<float>(m_height) / m_virtualHeight);

	m_viewportTransform.offsetX = (m_width - (m_virtualWidth * m_viewportTransform.scale)) / 2.0f;
	m_viewportTransform.offsetY = (m_height - (m_virtualHeight * m_viewportTransform.scale)) / 2.0f;

	if (m_renderedImage) {
		SDL_DestroySurface(m_renderedImage);
	}
	m_renderedImage = SDL_CreateSurface(m_width, m_height, SDL_PIXELFORMAT_RGBA32);

	if (m_uploadBuffer) {
		SDL_DestroyTexture(m_uploadBuffer);
	}
	m_uploadBuffer =
		SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, m_width, m_height);

	m_zBuffer.resize(m_width * m_height);
}

void Direct3DRMSoftwareRenderer::Clear(float r, float g, float b)
{
	const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(m_renderedImage->format);
	Uint32 color = SDL_MapRGB(details, m_palette, r * 255, g * 255, b * 255);
	SDL_FillSurfaceRect(m_renderedImage, nullptr, color);
}

void Direct3DRMSoftwareRenderer::Flip()
{
	SDL_UpdateTexture(m_uploadBuffer, nullptr, m_renderedImage->pixels, m_renderedImage->pitch);
	SDL_RenderTexture(m_renderer, m_uploadBuffer, nullptr, nullptr);
	SDL_RenderPresent(m_renderer);
}

void Direct3DRMSoftwareRenderer::Draw2DImage(
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
		Uint32 sdlColor = SDL_MapRGBA(
			m_format,
			m_palette,
			static_cast<Uint8>(color.r * 255),
			static_cast<Uint8>(color.g * 255),
			static_cast<Uint8>(color.b * 255),
			static_cast<Uint8>(color.a * 255)
		);
		SDL_FillSurfaceRect(m_renderedImage, &centeredRect, sdlColor);
		return;
	}

	bool isUpscaling = centeredRect.w > srcRect.w || centeredRect.h > srcRect.h;

	SDL_Surface* surface = m_textures[textureId].cached;
	SDL_UnlockSurface(surface);
	SDL_BlitSurfaceScaled(
		surface,
		&srcRect,
		m_renderedImage,
		&centeredRect,
		isUpscaling ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR
	);
	SDL_LockSurface(surface);
}

void Direct3DRMSoftwareRenderer::SetDither(bool dither)
{
}

void Direct3DRMSoftwareRenderer::SetPalette(SDL_Palette* palette)
{
	if (m_renderedImage) {
		SDL_SetSurfacePalette(m_renderedImage, palette);
	}
}

void Direct3DRMSoftwareRenderer::Download(SDL_Surface* target)
{
	SDL_Rect srcRect = {
		static_cast<int>(m_viewportTransform.offsetX),
		static_cast<int>(m_viewportTransform.offsetY),
		static_cast<int>(m_virtualWidth * m_viewportTransform.scale),
		static_cast<int>(m_virtualHeight * m_viewportTransform.scale),
	};

	SDL_BlitSurfaceScaled(m_renderedImage, &srcRect, target, nullptr, SDL_SCALEMODE_LINEAR);
}
