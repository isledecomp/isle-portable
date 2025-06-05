#pragma once

#include "d3drmrenderer.h"
#include "ddraw_impl.h"

#include <SDL3/SDL.h>

DEFINE_GUID(SDL3_GPU_GUID, 0x682656F3, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01);

typedef struct {
	D3DRMMATRIX4D projection;
	D3DRMMATRIX4D worldViewMatrix;
} ViewportUniforms;
static_assert(sizeof(ViewportUniforms) % 16 == 0);
static_assert(sizeof(ViewportUniforms) == 128);

struct FragmentShadingData {
	SceneLight lights[3];
	int lightCount;
	float shininess;
	SDL_Color color;
	int padding1[1];
};
static_assert(sizeof(FragmentShadingData) % 16 == 0);
static_assert(sizeof(FragmentShadingData) == 160);

class Direct3DRMSDL3GPURenderer : public Direct3DRMRenderer {
public:
	static Direct3DRMRenderer* Create(DWORD width, DWORD height);
	~Direct3DRMSDL3GPURenderer() override;
	void PushLights(const SceneLight* vertices, size_t count) override;
	Uint32 GetTextureId(IDirect3DRMTexture* texture) override;
	void SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back) override;
	DWORD GetWidth() override;
	DWORD GetHeight() override;
	void GetDesc(D3DDEVICEDESC* halDesc, D3DDEVICEDESC* helDesc) override;
	const char* GetName() override;
	HRESULT BeginFrame(const D3DRMMATRIX4D& viewMatrix) override;
	void SubmitDraw(
		const GeometryVertex* vertices,
		const size_t count,
		const D3DRMMATRIX4D& worldMatrix,
		const Matrix3x3& normalMatrix,
		const Appearance& appearance
	) override;
	HRESULT FinalizeFrame() override;

private:
	Direct3DRMSDL3GPURenderer(
		DWORD width,
		DWORD height,
		SDL_GPUDevice* device,
		SDL_GPUGraphicsPipeline* opaquePipeline,
		SDL_GPUGraphicsPipeline* transparentPipeline,
		SDL_GPUTexture* transferTexture,
		SDL_GPUTexture* depthTexture,
		SDL_GPUTransferBuffer* downloadTransferBuffer
	);
	HRESULT Blit();

	DWORD m_width;
	DWORD m_height;
	D3DVALUE m_front;
	D3DVALUE m_back;
	int m_vertexCount;
	int m_vertexBufferCount = 0;
	ViewportUniforms m_uniforms;
	FragmentShadingData m_fragmentShadingData;
	D3DDEVICEDESC m_desc;
	D3DRMMATRIX4D m_viewMatrix;
	SDL_GPUDevice* m_device;
	SDL_GPUGraphicsPipeline* m_opaquePipeline;
	SDL_GPUGraphicsPipeline* m_transparentPipeline;
	SDL_GPUTexture* m_transferTexture;
	SDL_GPUTexture* m_depthTexture;
	SDL_GPUTransferBuffer* m_downloadTransferBuffer;
	SDL_GPUBuffer* m_vertexBuffer = nullptr;
	SDL_Surface* m_renderedImage = nullptr;
};

inline static void Direct3DRMSDL3GPU_EnumDevice(LPD3DENUMDEVICESCALLBACK cb, void* ctx)
{
	Direct3DRMRenderer* device = Direct3DRMSDL3GPURenderer::Create(640, 480);
	if (device) {
		EnumDevice(cb, ctx, device, SDL3_GPU_GUID);
		delete device;
	}
}
