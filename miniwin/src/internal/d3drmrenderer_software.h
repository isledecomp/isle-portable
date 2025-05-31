#pragma once

#include "d3drmrenderer.h"

#include <SDL3/SDL.h>
#include <cstddef>
#include <vector>

DEFINE_GUID(SOFTWARE_GUID, 0x682656F3, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02);

class Direct3DRMSoftwareRenderer : public Direct3DRMRenderer {
public:
	Direct3DRMSoftwareRenderer(DWORD width, DWORD height);
	void SetBackbuffer(SDL_Surface* backbuffer) override;
	void PushVertices(const PositionColorVertex* vertices, size_t count) override;
	void PushLights(const SceneLight* vertices, size_t count) override;
	void SetProjection(D3DRMMATRIX4D perspective, D3DVALUE front, D3DVALUE back) override;
	DWORD GetWidth() override;
	DWORD GetHeight() override;
	void GetDesc(D3DDEVICEDESC* halDesc, D3DDEVICEDESC* helDesc) override;
	const char* GetName() override;
	HRESULT Render() override;

private:
	void ClearZBuffer();
	void DrawTriangleProjected(const PositionColorVertex&, const PositionColorVertex&, const PositionColorVertex&);
	void DrawTriangleClipped(
		const PositionColorVertex& v0,
		const PositionColorVertex& v1,
		const PositionColorVertex& v2
	);
	void ProjectVertex(const PositionColorVertex&, float&, float&, float&) const;
	void BlendPixel(Uint8* pixelAddr, const PositionColorVertex& srcColor);

	DWORD m_width;
	DWORD m_height;
	SDL_Surface* m_backbuffer = nullptr;
	SDL_Palette* m_palette;
	const SDL_PixelFormatDetails* m_format;
	int m_bytesPerPixel;
	D3DVALUE m_front;
	D3DVALUE m_back;
	std::vector<PositionColorVertex> m_vertexBuffer;
	float proj[4][4] = {0};
	std::vector<float> m_zBuffer;
};
