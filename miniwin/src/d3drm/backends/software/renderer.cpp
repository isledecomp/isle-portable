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
#elif (defined(__arm__) || defined(__aarch64__)) && !defined(__3DS__)
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

void Direct3DRMSoftwareRenderer::DrawTriangleClipped(const D3DRMVERTEX (&v)[3], const Appearance& appearance)
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
		D3DRMVERTEX split;
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
	const D3DVECTOR& oNormal,
	const Appearance& appearance
)
{
	FColor specular = {0, 0, 0, 0};
	FColor diffuse = {0, 0, 0, 0};

	D3DVECTOR normal = Normalize(TransformNormal(oNormal, m_normalMatrix));

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

inline D3DVECTOR Subtract(const D3DVECTOR& a, const D3DVECTOR& b)
{
	return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline bool IsBackface(const D3DVECTOR& v0, const D3DVECTOR& v1, const D3DVECTOR& v2)
{
	D3DVECTOR normal = CrossProduct(Subtract(v1, v0), Subtract(v2, v0));

	return DotProduct(normal, v0) >= 0.0f;
}

void Direct3DRMSoftwareRenderer::DrawTriangleProjected(
	const D3DRMVERTEX& v0,
	const D3DRMVERTEX& v1,
	const D3DRMVERTEX& v2,
	const Appearance& appearance
)
{
	if (IsBackface(v0.position, v1.position, v2.position)) {
		return;
	}

	D3DRMVECTOR4D p0, p1, p2;
	ProjectVertex(v0.position, p0);
	ProjectVertex(v1.position, p1);
	ProjectVertex(v2.position, p2);

	Uint8 r, g, b;
	SDL_Color c0 = ApplyLighting(v0.position, v0.normal, appearance);
	SDL_Color c1 = {}, c2 = {};
	if (!appearance.flat) {
		c1 = ApplyLighting(v1.position, v1.normal, appearance);
		c2 = ApplyLighting(v2.position, v2.normal, appearance);
	}

	Uint8* pixels = (Uint8*) m_renderedImage->pixels;
	int pitch = m_renderedImage->pitch;

	VertexXY verts[3] = {
		{p0.x, p0.y, p0.z, p0.w, c0, v0.texCoord.u, v0.texCoord.v},
		{p1.x, p1.y, p1.z, p1.w, c1, v1.texCoord.u, v1.texCoord.v},
		{p2.x, p2.y, p2.z, p2.w, c2, v2.texCoord.u, v2.texCoord.v}
	};

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
			if (appearance.flat) {
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
	m_bytesPerPixel = m_format->bits_per_pixel / 8;

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
