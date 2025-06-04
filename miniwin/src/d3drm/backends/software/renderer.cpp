#include "d3drmrenderer.h"
#include "d3drmrenderer_software.h"
#include "ddsurface_impl.h"
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

void Direct3DRMSoftwareRenderer::PushVertices(const PositionColorVertex* vertices, size_t count)
{
	if (!count) {
		return;
	}
	m_vertexBuffer.resize(count);
	memcpy(m_vertexBuffer.data(), vertices, count * sizeof(PositionColorVertex));
}

void Direct3DRMSoftwareRenderer::SetProjection(D3DRMMATRIX4D perspective, D3DVALUE front, D3DVALUE back)
{
	m_front = front;
	m_back = back;
	memcpy(proj, perspective, sizeof(proj));
}

void Direct3DRMSoftwareRenderer::ClearZBuffer()
{
	std::fill(m_zBuffer.begin(), m_zBuffer.end(), std::numeric_limits<float>::infinity());
}

void Direct3DRMSoftwareRenderer::ProjectVertex(const PositionColorVertex& v, D3DRMVECTOR4D& p) const
{
	float px = proj[0][0] * v.position.x + proj[1][0] * v.position.y + proj[2][0] * v.position.z + proj[3][0];
	float py = proj[0][1] * v.position.x + proj[1][1] * v.position.y + proj[2][1] * v.position.z + proj[3][1];
	float pz = proj[0][2] * v.position.x + proj[1][2] * v.position.y + proj[2][2] * v.position.z + proj[3][2];
	float pw = proj[0][3] * v.position.x + proj[1][3] * v.position.y + proj[2][3] * v.position.z + proj[3][3];

	p.w = pw;

	// Perspective divide
	if (pw != 0.0f) {
		px /= pw;
		py /= pw;
		pz /= pw;
	}

	// Map from NDC [-1,1] to screen coordinates
	p.x = (px * 0.5f + 0.5f) * m_width;
	p.y = (1.0f - (py * 0.5f + 0.5f)) * m_height;
	p.z = pz;
}

PositionColorVertex SplitEdge(PositionColorVertex a, const PositionColorVertex& b, float plane)
{
	float t = (plane - a.position.z) / (b.position.z - a.position.z);
	a.position.x = a.position.x + t * (b.position.x - a.position.x);
	a.position.y = a.position.y + t * (b.position.y - a.position.y);
	a.position.z = plane;

	a.texCoord.u = a.texCoord.u + t * (b.texCoord.u - a.texCoord.u);
	a.texCoord.v = a.texCoord.v + t * (b.texCoord.v - a.texCoord.v);

	a.normals.x = a.normals.x + t * (b.normals.x - a.normals.x);
	a.normals.y = a.normals.y + t * (b.normals.y - a.normals.y);
	a.normals.z = a.normals.z + t * (b.normals.z - a.normals.z);

	float len = std::sqrt(a.normals.x * a.normals.x + a.normals.y * a.normals.y + a.normals.z * a.normals.z);
	if (len > 0.0001f) {
		a.normals.x /= len;
		a.normals.y /= len;
		a.normals.z /= len;
	}

	return a;
}

void Direct3DRMSoftwareRenderer::DrawTriangleClipped(
	const PositionColorVertex& v0,
	const PositionColorVertex& v1,
	const PositionColorVertex& v2
)
{
	bool in0 = v0.position.z >= m_front;
	bool in1 = v1.position.z >= m_front;
	bool in2 = v2.position.z >= m_front;

	int insideCount = in0 + in1 + in2;

	if (insideCount == 0) {
		return;
	}

	if (insideCount == 3) {
		DrawTriangleProjected(v0, v1, v2);
	}
	else if (insideCount == 2) {
		PositionColorVertex split;
		if (!in0) {
			split = SplitEdge(v2, v0, m_front);
			DrawTriangleProjected(v1, v2, split);
			DrawTriangleProjected(v1, split, SplitEdge(v1, v0, m_front));
		}
		else if (!in1) {
			split = SplitEdge(v0, v1, m_front);
			DrawTriangleProjected(v2, v0, split);
			DrawTriangleProjected(v2, split, SplitEdge(v2, v1, m_front));
		}
		else {
			split = SplitEdge(v1, v2, m_front);
			DrawTriangleProjected(v0, v1, split);
			DrawTriangleProjected(v0, split, SplitEdge(v0, v2, m_front));
		}
	}
	else if (in0) {
		DrawTriangleProjected(v0, SplitEdge(v0, v1, m_front), SplitEdge(v0, v2, m_front));
	}
	else if (in1) {
		DrawTriangleProjected(SplitEdge(v1, v0, m_front), v1, SplitEdge(v1, v2, m_front));
	}
	else {
		DrawTriangleProjected(SplitEdge(v2, v0, m_front), SplitEdge(v2, v1, m_front), v2);
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

SDL_Color Direct3DRMSoftwareRenderer::ApplyLighting(const PositionColorVertex& vertex)
{
	FColor specular = {0, 0, 0, 0};
	FColor diffuse = {0, 0, 0, 0};

	// Position and normal
	D3DVECTOR position = vertex.position;
	D3DVECTOR normal = vertex.normals;
	float normLen = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
	if (normLen == 0.0f) {
		return vertex.colors;
	}

	normal.x /= normLen;
	normal.y /= normLen;
	normal.z /= normLen;

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

		float len = std::sqrt(lightVec.x * lightVec.x + lightVec.y * lightVec.y + lightVec.z * lightVec.z);
		if (len == 0.0f) {
			continue;
		}
		lightVec.x /= len;
		lightVec.y /= len;
		lightVec.z /= len;

		float dotNL = normal.x * lightVec.x + normal.y * lightVec.y + normal.z * lightVec.z;
		if (dotNL > 0.0f) {
			// Diffuse contribution
			diffuse.r += dotNL * lightColor.r;
			diffuse.g += dotNL * lightColor.g;
			diffuse.b += dotNL * lightColor.b;

			if (vertex.shininess != 0.0f) {
				// Using dotNL ignores view angle, but this matches DirectX 5 behavior.
				float spec = std::pow(dotNL, vertex.shininess);
				specular.r += spec * lightColor.r;
				specular.g += spec * lightColor.g;
				specular.b += spec * lightColor.b;
			}
		}
	}

	return SDL_Color{
		static_cast<Uint8>(std::min(255.0f, diffuse.r * vertex.colors.r + specular.r * 255.0f)),
		static_cast<Uint8>(std::min(255.0f, diffuse.g * vertex.colors.g + specular.g * 255.0f)),
		static_cast<Uint8>(std::min(255.0f, diffuse.b * vertex.colors.b + specular.b * 255.0f)),
		vertex.colors.a
	};
}

void Direct3DRMSoftwareRenderer::DrawTriangleProjected(
	const PositionColorVertex& v0,
	const PositionColorVertex& v1,
	const PositionColorVertex& v2
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

	int minX = std::max(0, (int) std::floor(std::min({p0.x, p1.x, p2.x})));
	int maxX = std::min((int) m_width - 1, (int) std::ceil(std::max({p0.x, p1.x, p2.x})));
	int minY = std::max(0, (int) std::floor(std::min({p0.y, p1.y, p2.y})));
	int maxY = std::min((int) m_height - 1, (int) std::ceil(std::max({p0.y, p1.y, p2.y})));
	if (minX > maxX || minY > maxY) {
		return;
	}

	auto edge = [](double x0, double y0, double x1, double y1, double x, double y) {
		return (x - x0) * (y1 - y0) - (y - y0) * (x1 - x0);
	};
	float area = edge(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y);
	if (area >= 0) {
		return;
	}
	float invArea = 1.0f / area;

	// Per-vertex lighting using vertex normals
	SDL_Color c0 = ApplyLighting(v0);
	SDL_Color c1 = ApplyLighting(v1);
	SDL_Color c2 = ApplyLighting(v2);

	Uint32 texId = v0.texId;
	int texturePitch;
	Uint8* texels = nullptr;
	int texWidthScale;
	int texHeightScale;
	if (texId != NO_TEXTURE_ID) {
		SDL_Surface* texture = m_textures[texId].cached;
		if (texture) {
			texturePitch = texture->pitch;
			texels = static_cast<Uint8*>(texture->pixels);
			texWidthScale = texture->w - 1;
			texHeightScale = texture->h - 1;
		}
	}

	Uint8* pixels = (Uint8*) DDBackBuffer->pixels;
	int pitch = DDBackBuffer->pitch;

	for (int y = minY; y <= maxY; ++y) {
		for (int x = minX; x <= maxX; ++x) {
			float px = x + 0.5f;
			float py = y + 0.5f;
			float w0 = edge(p1.x, p1.y, p2.x, p2.y, px, py) * invArea;
			if (w0 < 0.0f || w0 > 1.0f) {
				continue;
			}

			float w1 = edge(p2.x, p2.y, p0.x, p0.y, px, py) * invArea;
			if (w1 < 0.0f || w1 > 1.0f - w0) {
				continue;
			}

			float w2 = 1.0f - w0 - w1;
			float z = w0 * p0.z + w1 * p1.z + w2 * p2.z;

			int zidx = y * m_width + x;
			float& zref = m_zBuffer[zidx];
			if (z >= zref) {
				continue;
			}

			// Interpolate color
			Uint8 r = static_cast<Uint8>(w0 * c0.r + w1 * c1.r + w2 * c2.r);
			Uint8 g = static_cast<Uint8>(w0 * c0.g + w1 * c1.g + w2 * c2.g);
			Uint8 b = static_cast<Uint8>(w0 * c0.b + w1 * c1.b + w2 * c2.b);
			Uint8* pixelAddr = pixels + y * pitch + x * m_bytesPerPixel;

			if (v0.colors.a == 255) {
				zref = z;

				if (texels) {
					// Perspective correct interpolate texture coords
					float invW = w0 / p0.w + w1 / p1.w + w2 / p2.w;
					if (invW == 0.0) {
						continue;
					}
					invW = 1.0 / invW;
					float u = static_cast<float>(
						((w0 * v0.texCoord.u / p0.w) + (w1 * v1.texCoord.u / p1.w) + (w2 * v2.texCoord.u / p2.w)) * invW
					);
					float v = static_cast<float>(
						((w0 * v0.texCoord.v / p0.w) + (w1 * v1.texCoord.v / p1.w) + (w2 * v2.texCoord.v / p2.w)) * invW
					);

					// Tile textures
					u = u - std::floor(u);
					v = v - std::floor(v);

					int texX = static_cast<int>(u * texWidthScale);
					int texY = static_cast<int>(v * texHeightScale);

					Uint8* texelAddr = texels + texY * texturePitch + texX * m_bytesPerPixel;

					Uint32 texelColor = 0;
					memcpy(&texelColor, texelAddr, m_bytesPerPixel);

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
				BlendPixel(pixelAddr, r, g, b, v0.colors.a);
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
		if (texRef.texture == nullptr) {
			texRef.texture = texture;
			texRef.cached = convertedRender;
			texRef.version = texture->m_version;
			AddTextureDestroyCallback(i, texture);
			return i;
		}
	}

	// Append new
	m_textures.push_back({texture, texture->m_version, convertedRender});
	AddTextureDestroyCallback(static_cast<Uint32>(m_textures.size() - 1), texture);
	return static_cast<Uint32>(m_textures.size() - 1);
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

HRESULT Direct3DRMSoftwareRenderer::Render()
{
	if (!DDBackBuffer || m_vertexBuffer.size() % 3 != 0 || !SDL_LockSurface(DDBackBuffer)) {
		return DDERR_GENERIC;
	}
	ClearZBuffer();
	m_format = SDL_GetPixelFormatDetails(DDBackBuffer->format);
	m_palette = SDL_GetSurfacePalette(DDBackBuffer);
	m_bytesPerPixel = m_format->bits_per_pixel / 8;
	for (size_t i = 0; i + 2 < m_vertexBuffer.size(); i += 3) {
		DrawTriangleClipped(m_vertexBuffer[i], m_vertexBuffer[i + 1], m_vertexBuffer[i + 2]);
	}
	SDL_UnlockSurface(DDBackBuffer);

	m_vertexBuffer.clear();

	return DD_OK;
}
