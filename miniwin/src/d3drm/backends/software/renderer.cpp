#include "d3drmrenderer.h"
#include "d3drmrenderer_software.h"
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

void Direct3DRMSoftwareRenderer::SetBackbuffer(SDL_Surface* buf)
{
	m_backbuffer = buf;
}

void Direct3DRMSoftwareRenderer::PushLights(const SceneLight* vertices, size_t count)
{
	MINIWIN_NOT_IMPLEMENTED();
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

void Direct3DRMSoftwareRenderer::ProjectVertex(const PositionColorVertex& v, float& out_x, float& out_y, float& out_z)
	const
{
	float px = proj[0][0] * v.x + proj[1][0] * v.y + proj[2][0] * v.z + proj[3][0];
	float py = proj[0][1] * v.x + proj[1][1] * v.y + proj[2][1] * v.z + proj[3][1];
	float pz = proj[0][2] * v.x + proj[1][2] * v.y + proj[2][2] * v.z + proj[3][2];
	float pw = proj[0][3] * v.x + proj[1][3] * v.y + proj[2][3] * v.z + proj[3][3];

	// Perspective divide
	if (pw != 0.0f) {
		px /= pw;
		py /= pw;
		pz /= pw;
	}

	// Map from NDC [-1,1] to screen coordinates
	out_x = (px * 0.5f + 0.5f) * m_width;
	out_y = (1.0f - (py * 0.5f + 0.5f)) * m_height;
	out_z = pz;
}

PositionColorVertex SplitEdge(PositionColorVertex a, const PositionColorVertex& b, float plane)
{
	float t = (plane - a.z) / (b.z - a.z);
	a.x = a.x + t * (b.x - a.x);
	a.y = a.y + t * (b.y - a.y);
	a.z = plane;
	return a;
}

void Direct3DRMSoftwareRenderer::DrawTriangleClipped(
	const PositionColorVertex& v0,
	const PositionColorVertex& v1,
	const PositionColorVertex& v2
)
{
	bool in0 = v0.z >= m_front;
	bool in1 = v1.z >= m_front;
	bool in2 = v2.z >= m_front;

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
void Direct3DRMSoftwareRenderer::BlendPixel(Uint8* pixelAddr, const PositionColorVertex& srcColor)
{
	Uint32 dstPixel = 0;
	memcpy(&dstPixel, pixelAddr, m_bytesPerPixel);

	Uint8 dstR, dstG, dstB, dstA;
	SDL_GetRGBA(dstPixel, m_format, m_palette, &dstR, &dstG, &dstB, &dstA);

	float alpha = srcColor.a / 255.0f;
	float invAlpha = 1.0f - alpha;

	Uint8 outR = static_cast<Uint8>(srcColor.r * alpha + dstR * invAlpha);
	Uint8 outG = static_cast<Uint8>(srcColor.g * alpha + dstG * invAlpha);
	Uint8 outB = static_cast<Uint8>(srcColor.b * alpha + dstB * invAlpha);
	Uint8 outA = static_cast<Uint8>(srcColor.a + dstA * invAlpha);

	Uint32 blended = SDL_MapRGBA(m_format, m_palette, outR, outG, outB, outA);
	memcpy(pixelAddr, &blended, m_bytesPerPixel);
}

void Direct3DRMSoftwareRenderer::DrawTriangleProjected(
	const PositionColorVertex& v0,
	const PositionColorVertex& v1,
	const PositionColorVertex& v2
)
{
	float x0, y0, z0, x1, y1, z1, x2, y2, z2;
	ProjectVertex(v0, x0, y0, z0);
	ProjectVertex(v1, x1, y1, z1);
	ProjectVertex(v2, x2, y2, z2);

	// Skip triangles outside the frustum
	if ((z0 < m_front && z1 < m_front && z2 < m_front) || (z0 > m_back && z1 > m_back && z2 > m_back)) {
		return;
	}

	// Skip offscreen triangles
	if ((x0 < 0 && x1 < 0 && x2 < 0) || (x0 >= m_width && x1 >= m_width && x2 >= m_width) ||
		(y0 < 0 && y1 < 0 && y2 < 0) || (y0 >= m_height && y1 >= m_height && y2 >= m_height)) {
		return;
	}

	int minX = std::max(0, (int) std::floor(std::min({x0, x1, x2})));
	int maxX = std::min((int) m_width - 1, (int) std::ceil(std::max({x0, x1, x2})));
	int minY = std::max(0, (int) std::floor(std::min({y0, y1, y2})));
	int maxY = std::min((int) m_height - 1, (int) std::ceil(std::max({y0, y1, y2})));
	if (minX > maxX || minY > maxY) {
		return;
	}

	auto edge = [](float x0, float y0, float x1, float y1, float x, float y) {
		return (x - x0) * (y1 - y0) - (y - y0) * (x1 - x0);
	};
	float area = edge(x0, y0, x1, y1, x2, y2);
	if (area >= 0) {
		return;
	}
	float invArea = 1.0f / area;

	Uint32 color;
	if (v0.a == 255) {
		color = SDL_MapRGBA(m_format, m_palette, v0.r, v0.g, v0.b, v0.a);
	}

	Uint8* pixels = (Uint8*) m_backbuffer->pixels;
	int pitch = m_backbuffer->pitch;

	for (int y = minY; y <= maxY; ++y) {
		for (int x = minX; x <= maxX; ++x) {
			float px = x + 0.5f;
			float py = y + 0.5f;
			float w0 = edge(x1, y1, x2, y2, px, py) * invArea;
			if (w0 < 0.0f || w0 > 1.0f) {
				continue;
			}

			float w1 = edge(x2, y2, x0, y0, px, py) * invArea;
			if (w1 < 0.0f || w1 > 1.0f - w0) {
				continue;
			}

			float w2 = 1.0f - w0 - w1;
			float z = w0 * z0 + w1 * z1 + w2 * z2;

			int zidx = y * m_width + x;
			float& zref = m_zBuffer[zidx];
			if (z >= zref) {
				continue;
			}

			Uint8* pixelAddr = pixels + y * pitch + x * m_bytesPerPixel;
			if (v0.a == 255) {
				zref = z;
				memcpy(pixelAddr, &color, m_bytesPerPixel);
			}
			else {
				BlendPixel(pixelAddr, v0);
			}
		}
	}
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
	return "Software Renderer";
}

HRESULT Direct3DRMSoftwareRenderer::Render()
{
	if (!m_backbuffer || m_vertexBuffer.size() % 3 != 0 || !SDL_LockSurface(m_backbuffer)) {
		return DDERR_GENERIC;
	}
	ClearZBuffer();
	m_format = SDL_GetPixelFormatDetails(m_backbuffer->format);
	m_palette = SDL_GetSurfacePalette(m_backbuffer);
	m_bytesPerPixel = m_format->bits_per_pixel / 8;
	for (size_t i = 0; i + 2 < m_vertexBuffer.size(); i += 3) {
		DrawTriangleClipped(m_vertexBuffer[i], m_vertexBuffer[i + 1], m_vertexBuffer[i + 2]);
	}
	SDL_UnlockSurface(m_backbuffer);

	m_vertexBuffer.clear();

	return DD_OK;
}
