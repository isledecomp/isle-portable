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
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <xmmintrin.h>
#if defined(__i386__) || defined(_M_IX86)
#include <xmmintrin.h>
#endif
#endif
#if defined(__arm__) || defined(__aarch64__)
#include <arm_neon.h>
#endif
#if defined(__wasm_simd128__)
#include <wasm_simd128.h>
#endif

Direct3DRMSoftwareRenderer::Direct3DRMSoftwareRenderer(DWORD width, DWORD height) : m_width(width), m_height(height)
{
	m_zBuffer.resize(m_width * m_height);
}

void Direct3DRMSoftwareRenderer::PushLights(const SceneLight* lights, size_t count)
{
	m_lights.assign(lights, lights + count);
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
	const size_t size = m_zBuffer.size();
	const float inf = std::numeric_limits<float>::infinity();
	size_t i = 0;

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
	if (SDL_HasSSE2()) {
		__m128 inf4 = _mm_set1_ps(inf);
		for (; i + 4 <= size; i += 4) {
			_mm_storeu_ps(&m_zBuffer[i], inf4);
		}
	}
#if defined(__i386__) || defined(_M_IX86)
	else if (SDL_HasMMX()) {
		const __m64 mm_inf = _mm_set_pi32(0x7F800000, 0x7F800000);
		for (; i + 2 <= size; i += 2) {
			*reinterpret_cast<__m64*>(&m_zBuffer[i]) = mm_inf;
		}
		_mm_empty();
	}
#endif
#elif defined(__arm__) || defined(__aarch64__)
	if (SDL_HasNEON()) {
		float32x4_t inf4 = vdupq_n_f32(inf);
		for (; i + 4 <= size; i += 4) {
			vst1q_f32(&m_zBuffer[i], inf4);
		}
	}
#elif defined(__wasm_simd128__)
	const size_t simdWidth = 4;
	v128_t infVec = wasm_f32x4_splat(inf);
	for (; i + simdWidth <= size; i += simdWidth) {
		wasm_v128_store(&m_zBuffer[i], infVec);
	}
#endif

	for (; i < size; ++i) {
		m_zBuffer[i] = inf;
	}
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

D3DRMVERTEX SplitEdge(D3DRMVERTEX a, const D3DRMVERTEX& b, float plane)
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

Uint32 Direct3DRMSoftwareRenderer::BlendPixel(Uint8* pixelAddr, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	Uint32 dstPixel;
	switch (m_bytesPerPixel) {
	case 1:
		dstPixel = *pixelAddr;
		break;
	case 2:
		dstPixel = *(Uint16*) pixelAddr;
		break;
	case 4:
		dstPixel = *(Uint32*) pixelAddr;
		break;
	}

	Uint8 dstR, dstG, dstB, dstA;
	SDL_GetRGBA(dstPixel, m_format, m_palette, &dstR, &dstG, &dstB, &dstA);

	float alpha = a / 255.0f;
	float invAlpha = 1.0f - alpha;

	Uint8 outR = static_cast<Uint8>(r * alpha + dstR * invAlpha);
	Uint8 outG = static_cast<Uint8>(g * alpha + dstG * invAlpha);
	Uint8 outB = static_cast<Uint8>(b * alpha + dstB * invAlpha);
	Uint8 outA = static_cast<Uint8>(a + dstA * invAlpha);

	return SDL_MapRGBA(m_format, m_palette, outR, outG, outB, outA);
}

SDL_Color Direct3DRMSoftwareRenderer::ApplyLighting(
	const D3DVECTOR& position,
	const D3DVECTOR& normal,
	const Appearance& appearance
)
{
	FColor specular = {0, 0, 0, 0};
	FColor diffuse = {0, 0, 0, 0};

	for (const auto& light : m_lights) {
		FColor lightColor = light.color;

		if (light.positional == 0.0f && light.directional == 0.0f) {
			// Ambient light
			diffuse.r += lightColor.r;
			diffuse.g += lightColor.g;
			diffuse.b += lightColor.b;
			continue;
		}

		// TODO lightVec only has to be calculated once per frame for directional lights
		D3DVECTOR lightVec;
		if (light.directional == 1.0f) {
			lightVec = {-light.direction.x, -light.direction.y, -light.direction.z};
		}
		else if (light.positional == 1.0f) {
			lightVec = {light.position.x - position.x, light.position.y - position.y, light.position.z - position.z};
		}
		lightVec = Normalize(lightVec);

		float dotNL = DotProduct(normal, lightVec);
		if (dotNL > 0.0f) {
			// Diffuse contribution
			diffuse.r += dotNL * lightColor.r;
			diffuse.g += dotNL * lightColor.g;
			diffuse.b += dotNL * lightColor.b;

			// Specular
			if (appearance.shininess > 0.0f && light.directional == 1.0f) {
				D3DVECTOR viewVec = Normalize({-position.x, -position.y, -position.z});
				D3DVECTOR H = Normalize({lightVec.x + viewVec.x, lightVec.y + viewVec.y, lightVec.z + viewVec.z});

				float dotNH = std::max(DotProduct(normal, H), 0.0f);
				float spec = std::pow(dotNH, appearance.shininess);

				specular.r += spec * lightColor.r;
				specular.g += spec * lightColor.g;
				specular.b += spec * lightColor.b;
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

struct VertexXY {
	float x, y, z, w;
	SDL_Color color;
	float u_over_w, v_over_w;
	float one_over_w;
};

VertexXY InterpolateVertex(float y, const VertexXY& v0, const VertexXY& v1)
{
	float dy = v1.y - v0.y;
	if (fabsf(dy) < 1e-6f) {
		dy = 1e-6f;
	}
	float t = (y - v0.y) / dy;
	VertexXY r;
	r.x = v0.x + t * (v1.x - v0.x);
	r.z = v0.z + t * (v1.z - v0.z);
	r.w = v0.w + t * (v1.w - v0.w);
	r.color.r = static_cast<Uint8>(v0.color.r + t * (v1.color.r - v0.color.r));
	r.color.g = static_cast<Uint8>(v0.color.g + t * (v1.color.g - v0.color.g));
	r.color.b = static_cast<Uint8>(v0.color.b + t * (v1.color.b - v0.color.b));
	r.color.a = static_cast<Uint8>(v0.color.a + t * (v1.color.a - v0.color.a));

	r.u_over_w = v0.u_over_w + t * (v1.u_over_w - v0.u_over_w);
	r.v_over_w = v0.v_over_w + t * (v1.v_over_w - v0.v_over_w);
	r.one_over_w = v0.one_over_w + t * (v1.one_over_w - v0.one_over_w);

	return r;
}

void Direct3DRMSoftwareRenderer::DrawTriangle(
	const D3DRMVECTOR4D& p0,
	const D3DRMVECTOR4D& p1,
	const D3DRMVECTOR4D& p2,
	const TexCoord& t0,
	const TexCoord& t1,
	const TexCoord& t2,
	const SDL_Color& c0,
	const SDL_Color& c1,
	const SDL_Color& c2,
	const Appearance& appearance,
	bool flat
)
{
	Uint8* pixels = (Uint8*) DDBackBuffer->pixels;
	int pitch = DDBackBuffer->pitch;

	VertexXY verts[3] = {
		{p0.x, p0.y, p0.z, p0.w, c0, t0.u, t0.v},
		{p1.x, p1.y, p1.z, p1.w, c1, t1.u, t1.v},
		{p2.x, p2.y, p2.z, p2.w, c2, t2.u, t2.v}
	};

	Uint8 r, g, b;

	Uint32 textureId = appearance.textureId;
	int texturePitch;
	Uint8* texels = nullptr;
	int texWidthScale;
	int texHeightScale;
	if (textureId != NO_TEXTURE_ID) {
		SDL_Surface* texture = m_textures[textureId].cached;
		if (texture) {
			texturePitch = texture->pitch;
			texels = static_cast<Uint8*>(texture->pixels);
			texWidthScale = texture->w - 1;
			texHeightScale = texture->h - 1;
		}

		verts[0].u_over_w = t0.u / p0.w;
		verts[0].v_over_w = t0.v / p0.w;
		verts[0].one_over_w = 1.0f / p0.w;

		verts[1].u_over_w = t1.u / p1.w;
		verts[1].v_over_w = t1.v / p1.w;
		verts[1].one_over_w = 1.0f / p1.w;

		verts[2].u_over_w = t2.u / p2.w;
		verts[2].v_over_w = t2.v / p2.w;
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

	for (int y = minY; y <= maxY; ++y) {
		VertexXY left, right;
		if (y < verts[1].y) {
			left = InterpolateVertex(y, verts[0], verts[1]);
			right = InterpolateVertex(y, verts[0], verts[2]);
		}
		else {
			left = InterpolateVertex(y, verts[1], verts[2]);
			right = InterpolateVertex(y, verts[0], verts[2]);
		}

		if (left.x > right.x) {
			std::swap(left, right);
		}

		int startX = std::max(0, (int) std::ceil(left.x));
		int endX = std::min((int) m_width - 1, (int) std::floor(right.x));

		float span = right.x - left.x;
		if (span == 0.0f) {
			continue;
		}

		for (int x = startX; x <= endX; ++x) {
			float t = (x - left.x) / span;
			float z = left.z + t * (right.z - left.z);

			int zidx = y * m_width + x;
			float& zref = m_zBuffer[zidx];
			if (z >= zref) {
				continue;
			}

			Uint8 r, g, b;
			if (flat) {
				r = c0.r;
				g = c0.g;
				b = c0.b;
			}
			else {
				r = static_cast<Uint8>(left.color.r + t * (right.color.r - left.color.r));
				g = static_cast<Uint8>(left.color.g + t * (right.color.g - left.color.g));
				b = static_cast<Uint8>(left.color.b + t * (right.color.b - left.color.b));
			}

			Uint8* pixelAddr = pixels + y * pitch + x * m_bytesPerPixel;
			Uint32 finalColor;

			if (appearance.color.a == 255) {
				zref = z;

				if (texels) {
					// Perspective correct interpolate texture coords
					float one_over_w = left.one_over_w + t * (right.one_over_w - left.one_over_w);
					float u_over_w = left.u_over_w + t * (right.u_over_w - left.u_over_w);
					float v_over_w = left.v_over_w + t * (right.v_over_w - left.v_over_w);

					float inv_w = 1.0f / one_over_w;
					float u = u_over_w * inv_w;
					float v = v_over_w * inv_w;

					// Tile textures
					u -= std::floor(u);
					v -= std::floor(v);

					int texX = static_cast<int>(u * texWidthScale);
					int texY = static_cast<int>(v * texHeightScale);

					Uint8* texelAddr = texels + texY * texturePitch + texX * m_bytesPerPixel;

					Uint32 texelColor;
					switch (m_bytesPerPixel) {
					case 1:
						texelColor = *texelAddr;
						break;
					case 2:
						texelColor = *(Uint16*) texelAddr;
						break;
					case 4:
						texelColor = *(Uint32*) texelAddr;
						break;
					}

					Uint8 tr, tg, tb, ta;
					SDL_GetRGBA(texelColor, m_format, m_palette, &tr, &tg, &tb, &ta);

					// Multiply vertex color by texel color
					r = (r * tr + 127) / 255;
					g = (g * tg + 127) / 255;
					b = (b * tb + 127) / 255;
				}

				finalColor = SDL_MapRGBA(m_format, m_palette, r, g, b, 255);
			}
			else {
				finalColor = BlendPixel(pixelAddr, r, g, b, appearance.color.a);
			}

			switch (m_bytesPerPixel) {
			case 1:
				*pixelAddr = static_cast<Uint8>(finalColor);
				break;
			case 2:
				*reinterpret_cast<Uint16*>(pixelAddr) = static_cast<Uint16>(finalColor);
				break;
			case 4:
				*reinterpret_cast<Uint32*>(pixelAddr) = finalColor;
				break;
			}
		}
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

Uint32 Direct3DRMSoftwareRenderer::GetTextureId(IDirect3DRMTexture* iTexture)
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
				texRef.cached = SDL_ConvertSurface(surface->m_surface, DDBackBuffer->format);
				SDL_LockSurface(texRef.cached);
				texRef.version = texture->m_version;
			}
			return i;
		}
	}

	SDL_Surface* convertedRender = SDL_ConvertSurface(surface->m_surface, DDBackBuffer->format);
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

DWORD Direct3DRMSoftwareRenderer::GetWidth()
{
	return m_width;
}

DWORD Direct3DRMSoftwareRenderer::GetHeight()
{
	return m_height;
}

void Direct3DRMSoftwareRenderer::GetDesc(D3DDEVICEDESC* halDesc, D3DDEVICEDESC* helDesc)
{
	memset(halDesc, 0, sizeof(D3DDEVICEDESC));

	helDesc->dcmColorModel = D3DCOLORMODEL::RGB;
	helDesc->dwFlags = D3DDD_DEVICEZBUFFERBITDEPTH;
	helDesc->dwDeviceZBufferBitDepth = DDBD_32;
	helDesc->dwDeviceRenderBitDepth = DDBD_8 | DDBD_16 | DDBD_24 | DDBD_32;
	helDesc->dpcTriCaps.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE;
	helDesc->dpcTriCaps.dwShadeCaps = D3DPSHADECAPS_ALPHAFLATBLEND;
	helDesc->dpcTriCaps.dwTextureFilterCaps = D3DPTFILTERCAPS_LINEAR;
}

const char* Direct3DRMSoftwareRenderer::GetName()
{
	return "Miniwin Emulation";
}

HRESULT Direct3DRMSoftwareRenderer::BeginFrame()
{
	if (!DDBackBuffer || !SDL_LockSurface(DDBackBuffer)) {
		return DDERR_GENERIC;
	}
	ClearZBuffer();

	m_format = SDL_GetPixelFormatDetails(DDBackBuffer->format);
	m_palette = SDL_GetSurfacePalette(DDBackBuffer);
	m_bytesPerPixel = m_format->bits_per_pixel / 8;

	return DD_OK;
}

void Direct3DRMSoftwareRenderer::EnableTransparency()
{
}

inline D3DVECTOR Subtract(const D3DVECTOR& a, const D3DVECTOR& b)
{
	return {a.x - b.x, a.y - b.y, a.z - b.z};
}

bool IsTriangleOutsideViewCone(
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

inline bool IsBackface(const D3DVECTOR& v0, const D3DVECTOR& v1, const D3DVECTOR& v2)
{
	D3DVECTOR normal = CrossProduct(Subtract(v1, v0), Subtract(v2, v0));

	return DotProduct(normal, v0) >= 0.0f;
}

uint16_t Direct3DRMSoftwareRenderer::RemapVertex(uint16_t oldIdx, const std::vector<D3DRMVERTEX>& meshVertices)
{
	if (m_indexRemap[oldIdx] == static_cast<uint16_t>(-1)) {
		m_indexRemap[oldIdx] = m_cleanVertices.size();
		const D3DRMVERTEX& src = meshVertices[oldIdx];
		m_cleanVertices.push_back(D3DRMVERTEX{m_transformedVerts[oldIdx], src.normal, src.texCoord});
	}
	return m_indexRemap[oldIdx];
}

uint16_t Direct3DRMSoftwareRenderer::PushVertex(const D3DRMVERTEX& v)
{
	uint16_t idx = m_cleanVertices.size();
	m_cleanVertices.push_back(v);
	return idx;
}

void Direct3DRMSoftwareRenderer::SubmitClippedTriangle(
	const D3DRMVERTEX& v0,
	const D3DRMVERTEX& v1,
	const D3DRMVERTEX& v2,
	uint16_t i0,
	const std::vector<D3DRMVERTEX>& originalVerts
)
{
	D3DRMVERTEX n0 = SplitEdge(v0, v1, m_front);
	D3DRMVERTEX n1 = SplitEdge(v0, v2, m_front);

	if (!IsBackface(v0.position, n0.position, n1.position)) {
		uint16_t r0 = RemapVertex(i0, originalVerts);
		uint16_t r1 = PushVertex(n0);
		uint16_t r2 = PushVertex(n1);
		m_newIndices.insert(m_newIndices.end(), {r0, r1, r2});
	}
}

void Direct3DRMSoftwareRenderer::SubmitClippedQuad(
	const D3DRMVERTEX& kept1,
	const D3DRMVERTEX& kept2,
	const D3DRMVERTEX& clipped,
	uint16_t kept1Idx,
	uint16_t kept2Idx,
	const std::vector<D3DRMVERTEX>& originalVerts
)
{
	// MATCH ORIGINAL SPLIT ORDER
	D3DRMVERTEX n0 = SplitEdge(kept2, clipped, m_front);
	D3DRMVERTEX n1 = SplitEdge(kept1, clipped, m_front);

	// Use actual triangle vertices for backface test
	if (!IsBackface(kept1.position, kept2.position, n0.position)) {
		uint16_t i0 = RemapVertex(kept1Idx, originalVerts);
		uint16_t i1 = RemapVertex(kept2Idx, originalVerts);
		uint16_t i2 = PushVertex(n0);
		uint16_t i3 = PushVertex(n1);
		m_newIndices.insert(m_newIndices.end(), {i0, i1, i2, i0, i2, i3});
	}
}

void Direct3DRMSoftwareRenderer::SubmitDraw(
	DWORD meshId,
	const D3DRMMATRIX4D& modelViewMatrix,
	const Matrix3x3& normalMatrix,
	const Appearance& appearance
)
{
	auto& mesh = m_meshs[meshId];

	// Pre-transform all vertex positions and normals
	m_transformedVerts.clear();
	m_transformedVerts.reserve(mesh.vertices.size());
	for (const auto& v : mesh.vertices) {
		m_transformedVerts.push_back(TransformPoint(v.position, modelViewMatrix));
	}

	m_cleanVertices.clear();
	m_newIndices.clear();

	m_indexRemap.assign(m_transformedVerts.size(), static_cast<uint16_t>(-1));

	for (size_t i = 0; i < mesh.indices.size(); i += 3) {
		size_t i0 = mesh.indices[i + 0];
		size_t i1 = mesh.indices[i + 1];
		size_t i2 = mesh.indices[i + 2];

		auto& d0 = m_transformedVerts[i0];
		auto& d1 = m_transformedVerts[i1];
		auto& d2 = m_transformedVerts[i2];

		bool in0 = d0.z >= m_front;
		bool in1 = d1.z >= m_front;
		bool in2 = d2.z >= m_front;

		int insideCount = in0 + in1 + in2;

		if (insideCount == 0 || d0.z > m_back && d1.z > m_back && d2.z > m_back) {
			continue; // Outside clipping
		}
		if (IsTriangleOutsideViewCone(d0, d1, d2, m_frustumPlanes)) {
			continue;
		}

		if (insideCount == 3) {
			if (!IsBackface(d0, d1, d2)) {
				m_newIndices.push_back(RemapVertex(i0, mesh.vertices));
				m_newIndices.push_back(RemapVertex(i1, mesh.vertices));
				m_newIndices.push_back(RemapVertex(i2, mesh.vertices));
			}
			continue;
		}

		const D3DRMVERTEX& o0 = mesh.vertices[i0];
		const D3DRMVERTEX& o1 = mesh.vertices[i1];
		const D3DRMVERTEX& o2 = mesh.vertices[i2];
		D3DRMVERTEX v0 = {d0, o0.normal, o0.texCoord};
		D3DRMVERTEX v1 = {d1, o1.normal, o1.texCoord};
		D3DRMVERTEX v2 = {d2, o2.normal, o2.texCoord};

		if (insideCount == 1) {
			if (in0) {
				SubmitClippedTriangle(v0, v1, v2, i0, mesh.vertices);
			}
			else if (in1) {
				SubmitClippedTriangle(v1, v2, v0, i1, mesh.vertices);
			}
			else { // in2
				SubmitClippedTriangle(v2, v0, v1, i2, mesh.vertices);
			}
			continue;
		}

		if (!in0) {
			SubmitClippedQuad(v1, v2, v0, i1, i2, mesh.vertices);
		}
		else if (!in1) {
			SubmitClippedQuad(v2, v0, v1, i2, i0, mesh.vertices);
		}
		else { // !in2
			SubmitClippedQuad(v0, v1, v2, i0, i1, mesh.vertices);
		}
	}

	m_lighting.resize(m_cleanVertices.size());
	m_projectedVerts.resize(m_cleanVertices.size());
	if (!mesh.flat) {
		for (size_t i = 0; i < m_cleanVertices.size(); ++i) {
			const D3DRMVERTEX& v = m_cleanVertices[i];
			m_lighting[i] = ApplyLighting(v.position, Normalize(TransformNormal(v.normal, normalMatrix)), appearance);
			ProjectVertex(v.position, m_projectedVerts[i]);
		}
	}
	else {
		for (size_t i = 0; i < m_cleanVertices.size(); ++i) {
			ProjectVertex(m_cleanVertices[i].position, m_projectedVerts[i]);
		}
		for (size_t i = 0; i + 2 < m_newIndices.size(); i += 3) {
			uint16_t firstIndex = m_newIndices[i];
			const D3DRMVERTEX& v = m_cleanVertices[firstIndex];
			m_lighting[firstIndex] =
				ApplyLighting(v.position, Normalize(TransformNormal(v.normal, normalMatrix)), appearance);
		}
	}

	// Assemble triangles using index buffer
	for (size_t i = 0; i + 2 < m_newIndices.size(); i += 3) {
		uint16_t i0 = m_newIndices[i + 0];
		uint16_t i1 = m_newIndices[i + 1];
		uint16_t i2 = m_newIndices[i + 2];

		DrawTriangle(
			m_projectedVerts[i0],
			m_projectedVerts[i1],
			m_projectedVerts[i2],
			m_cleanVertices[i0].texCoord,
			m_cleanVertices[i1].texCoord,
			m_cleanVertices[i2].texCoord,
			m_lighting[i0],
			m_lighting[i1],
			m_lighting[i2],
			appearance,
			mesh.flat
		);
	}
}

HRESULT Direct3DRMSoftwareRenderer::FinalizeFrame()
{
	SDL_UnlockSurface(DDBackBuffer);

	return DD_OK;
}
