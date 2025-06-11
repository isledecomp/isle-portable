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

Direct3DRMSoftwareRenderer::Direct3DRMSoftwareRenderer(DWORD width, DWORD height) : m_width(width), m_height(height)
{
	m_zBuffer.resize(m_width * m_height);
}

void Direct3DRMSoftwareRenderer::PushLights(const SceneLight* lights, size_t count)
{
	m_lights.assign(lights, lights + count);
}

void Direct3DRMSoftwareRenderer::SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back)
{
	m_front = front;
	m_back = back;
	memcpy(m_projection, projection, sizeof(D3DRMMATRIX4D));
}

void Direct3DRMSoftwareRenderer::ClearZBuffer()
{
	std::fill(m_zBuffer.begin(), m_zBuffer.end(), std::numeric_limits<float>::infinity());
}

void Direct3DRMSoftwareRenderer::ProjectVertex(const D3DRMVERTEX& v, D3DRMVECTOR4D& p) const
{
	float px = m_projection[0][0] * v.position.x + m_projection[1][0] * v.position.y +
			   m_projection[2][0] * v.position.z + m_projection[3][0];
	float py = m_projection[0][1] * v.position.x + m_projection[1][1] * v.position.y +
			   m_projection[2][1] * v.position.z + m_projection[3][1];
	float pz = m_projection[0][2] * v.position.x + m_projection[1][2] * v.position.y +
			   m_projection[2][2] * v.position.z + m_projection[3][2];
	float pw = m_projection[0][3] * v.position.x + m_projection[1][3] * v.position.y +
			   m_projection[2][3] * v.position.z + m_projection[3][3];

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

void Direct3DRMSoftwareRenderer::DrawTriangleClipped(const D3DRMVERTEX (&v)[3], const Appearance& appearance)
{
	bool in0 = v[0].position.z >= m_front;
	bool in1 = v[1].position.z >= m_front;
	bool in2 = v[2].position.z >= m_front;

	int insideCount = in0 + in1 + in2;

	if (insideCount == 0) {
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

/**
 * @todo pre-compute a blending table when running in 256 colors since the game always uses an alpha of 152
 */
void Direct3DRMSoftwareRenderer::BlendPixel(Uint8* pixelAddr, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	Uint32 dstPixel = 0;
	memcpy(&dstPixel, pixelAddr, m_bytesPerPixel);

	Uint8 dstR, dstG, dstB, dstA;
	SDL_GetRGBA(dstPixel, m_format, m_palette, &dstR, &dstG, &dstB, &dstA);

	float alpha = a / 255.0f;
	float invAlpha = 1.0f - alpha;

	Uint8 outR = static_cast<Uint8>(r * alpha + dstR * invAlpha);
	Uint8 outG = static_cast<Uint8>(g * alpha + dstG * invAlpha);
	Uint8 outB = static_cast<Uint8>(b * alpha + dstB * invAlpha);
	Uint8 outA = static_cast<Uint8>(a + dstA * invAlpha);

	Uint32 blended = SDL_MapRGBA(m_format, m_palette, outR, outG, outB, outA);
	memcpy(pixelAddr, &blended, m_bytesPerPixel);
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

		float dotNL = normal.x * lightVec.x + normal.y * lightVec.y + normal.z * lightVec.z;
		if (dotNL > 0.0f) {
			// Diffuse contribution
			diffuse.r += dotNL * lightColor.r;
			diffuse.g += dotNL * lightColor.g;
			diffuse.b += dotNL * lightColor.b;

			if (appearance.shininess != 0.0f) {
				// Using dotNL ignores view angle, but this matches DirectX 5 behavior.
				float spec = std::pow(dotNL, appearance.shininess * m_shininessFactor);
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

void Direct3DRMSoftwareRenderer::DrawTriangleProjected(
	const D3DRMVERTEX& v0,
	const D3DRMVERTEX& v1,
	const D3DRMVERTEX& v2,
	const Appearance& appearance
)
{
	D3DRMVECTOR4D p0, p1, p2;
	ProjectVertex(v0, p0);
	ProjectVertex(v1, p1);
	ProjectVertex(v2, p2);

	// Skip triangles outside the frustum
	if ((p0.z < m_front && p1.z < m_front && p2.z < m_front) || (p0.z > m_back && p1.z > m_back && p2.z > m_back)) {
		return;
	}

	// Skip offscreen triangles
	if ((p0.x < 0 && p1.x < 0 && p2.x < 0) || (p0.x >= m_width && p1.x >= m_width && p2.x >= m_width) ||
		(p0.y < 0 && p1.y < 0 && p2.y < 0) || (p0.y >= m_height && p1.y >= m_height && p2.y >= m_height)) {
		return;
	}

	// Cull backfaces
	if ((p2.x - p0.x) * (p1.y - p0.y) - (p2.y - p0.y) * (p1.x - p0.x) >= 0) {
		return;
	}

	Uint8 r, g, b;
	SDL_Color c0 = ApplyLighting(v0.position, v0.normal, appearance);
	SDL_Color c1, c2;
	if (!appearance.flat) {
		c1 = ApplyLighting(v1.position, v1.normal, appearance);
		c2 = ApplyLighting(v2.position, v2.normal, appearance);
	}

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
	}

	Uint8* pixels = (Uint8*) DDBackBuffer->pixels;
	int pitch = DDBackBuffer->pitch;

	VertexXY verts[3] = {
		{p0.x, p0.y, p0.z, p0.w, c0, v0.texCoord.u, v0.texCoord.v},
		{p1.x, p1.y, p1.z, p1.w, c1, v1.texCoord.u, v1.texCoord.v},
		{p2.x, p2.y, p2.z, p2.w, c2, v2.texCoord.u, v2.texCoord.v}
	};

	verts[0].u_over_w = v0.texCoord.u / p0.w;
	verts[0].v_over_w = v0.texCoord.v / p0.w;
	verts[0].one_over_w = 1.0f / p0.w;

	verts[1].u_over_w = v1.texCoord.u / p1.w;
	verts[1].v_over_w = v1.texCoord.v / p1.w;
	verts[1].one_over_w = 1.0f / p1.w;

	verts[2].u_over_w = v2.texCoord.u / p2.w;
	verts[2].v_over_w = v2.texCoord.v / p2.w;
	verts[2].one_over_w = 1.0f / p2.w;

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
					case 3:
						// Manually build the 24-bit color (assuming byte order)
						texelColor = texelAddr[0] | (texelAddr[1] << 8) | (texelAddr[2] << 16);
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

				Uint32 finalColor = SDL_MapRGBA(m_format, m_palette, r, g, b, 255);
				memcpy(pixelAddr, &finalColor, m_bytesPerPixel);
			}
			else {
				// Transparent alpha blending with vertex alpha
				BlendPixel(pixelAddr, r, g, b, appearance.color.a);
			}
		}
	}
}

struct TextureDestroyContext {
	Direct3DRMSoftwareRenderer* renderer;
	Uint32 textureId;
};

void Direct3DRMSoftwareRenderer::AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture)
{
	auto* ctx = new TextureDestroyContext{this, id};
	texture->AddDestroyCallback(
		[](IDirect3DRMObject* obj, void* arg) {
			auto* ctx = static_cast<TextureDestroyContext*>(arg);
			auto& cacheEntry = ctx->renderer->m_textures[ctx->textureId];
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

	std::vector<D3DRMVERTEX> vertices;
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

struct MeshDestroyContext {
	Direct3DRMSoftwareRenderer* renderer;
	Uint32 id;
};

void Direct3DRMSoftwareRenderer::AddMeshDestroyCallback(Uint32 id, IDirect3DRMMesh* mesh)
{
	auto* ctx = new MeshDestroyContext{this, id};
	mesh->AddDestroyCallback(
		[](IDirect3DRMObject*, void* arg) {
			auto* ctx = static_cast<MeshDestroyContext*>(arg);
			ctx->renderer->m_meshs[ctx->id].meshGroup = nullptr;
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

HRESULT Direct3DRMSoftwareRenderer::BeginFrame(const D3DRMMATRIX4D& viewMatrix)
{
	if (!DDBackBuffer || !SDL_LockSurface(DDBackBuffer)) {
		return DDERR_GENERIC;
	}
	ClearZBuffer();

	memcpy(m_viewMatrix, viewMatrix, sizeof(D3DRMMATRIX4D));
	m_format = SDL_GetPixelFormatDetails(DDBackBuffer->format);
	m_palette = SDL_GetSurfacePalette(DDBackBuffer);
	m_bytesPerPixel = m_format->bits_per_pixel / 8;

	return DD_OK;
}

void Direct3DRMSoftwareRenderer::EnableTransparency()
{
}

void Direct3DRMSoftwareRenderer::SubmitDraw(
	DWORD meshId,
	const D3DRMMATRIX4D& worldMatrix,
	const Matrix3x3& normalMatrix,
	const Appearance& appearance
)
{
	D3DRMMATRIX4D mvMatrix;
	MultiplyMatrix(mvMatrix, worldMatrix, m_viewMatrix);

	auto& mesh = m_meshs[meshId];

	// Pre-transform all vertex positions and normals
	std::vector<D3DRMVERTEX> transformedVerts(mesh.vertices.size());
	for (size_t i = 0; i < mesh.vertices.size(); ++i) {
		const D3DRMVERTEX& src = mesh.vertices[i];
		D3DRMVERTEX& dst = transformedVerts[i];
		dst.position = TransformPoint(src.position, mvMatrix);
		// TODO defer normal transformation til lighting to allow culling first
		dst.normal = Normalize(TransformNormal(src.normal, normalMatrix));
		dst.texCoord = src.texCoord;
	}

	// Assemble triangles using index buffer
	for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
		DrawTriangleClipped(
			{transformedVerts[mesh.indices[i]],
			 transformedVerts[mesh.indices[i + 1]],
			 transformedVerts[mesh.indices[i + 2]]},
			appearance
		);
	}
}

HRESULT Direct3DRMSoftwareRenderer::FinalizeFrame()
{
	SDL_UnlockSurface(DDBackBuffer);

	return DD_OK;
}
