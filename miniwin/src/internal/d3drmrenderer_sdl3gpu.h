#pragma once

#include "d3drmrenderer.h"

#include <SDL3/SDL.h>

DEFINE_GUID(SDL3_GPU_GUID, 0x682656F3, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01);

typedef struct {
	D3DRMMATRIX4D perspective;
} ViewportUniforms;

struct SceneLights {
	SceneLight lights[3];
	int count;
};

class Direct3DRMSDL3GPURenderer : public Direct3DRMRenderer {
public:
	static Direct3DRMRenderer* Create(DWORD width, DWORD height);
	~Direct3DRMSDL3GPURenderer() override;
	void SetBackbuffer(SDL_Surface* backbuffer) override;
	void PushVertices(const PositionColorVertex* vertices, size_t count) override;
	void PushLights(const SceneLight* vertices, size_t count) override;
	Uint32 GetTextureId(IDirect3DRMTexture* texture) override;
	void SetProjection(D3DRMMATRIX4D perspective, D3DVALUE front, D3DVALUE back) override;
	DWORD GetWidth() override;
	DWORD GetHeight() override;
	void GetDesc(D3DDEVICEDESC* halDesc, D3DDEVICEDESC* helDesc) override;
	const char* GetName() override;
	HRESULT Render() override;

private:
	Direct3DRMSDL3GPURenderer(
		DWORD width,
		DWORD height,
		SDL_GPUDevice* device,
		SDL_GPUGraphicsPipeline* pipeline,
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
	SceneLights m_lights;
	D3DDEVICEDESC m_desc;
	SDL_Surface* m_backbuffer = nullptr;
	SDL_GPUDevice* m_device;
	SDL_GPUGraphicsPipeline* m_pipeline;
	SDL_GPUTexture* m_transferTexture;
	SDL_GPUTexture* m_depthTexture;
	SDL_GPUTransferBuffer* m_downloadTransferBuffer;
	SDL_GPUBuffer* m_vertexBuffer = nullptr;
	SDL_Surface* m_renderedImage = nullptr;
};
