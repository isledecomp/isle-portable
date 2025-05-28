#pragma once

#include "miniwin_d3drm.h"
#include "miniwin_d3drmobject_sdl3gpu.h"

#include <SDL3/SDL.h>

class Direct3DRMDevice_SDL3GPUImpl;
class Direct3DRMFrame_SDL3GPUImpl;

typedef struct {
	D3DRMMATRIX4D perspective;
} ViewportUniforms;

struct Direct3DRMViewport_SDL3GPUImpl : public Direct3DRMObjectBase_SDL3GPUImpl<IDirect3DRMViewport> {
	Direct3DRMViewport_SDL3GPUImpl(
		DWORD width,
		DWORD height,
		SDL_GPUDevice* device,
		SDL_GPUTexture* transferTexture,
		SDL_GPUTexture* depthTexture,
		SDL_GPUTransferBuffer* downloadTransferBuffer,
		SDL_GPUGraphicsPipeline* pipeline
	);
	~Direct3DRMViewport_SDL3GPUImpl() override;
	HRESULT Render(IDirect3DRMFrame* group) override;
	/**
	 * @brief Blit the render back to our backbuffer
	 */
	HRESULT ForceUpdate(int x, int y, int w, int h) override;
	HRESULT Clear() override;
	HRESULT SetCamera(IDirect3DRMFrame* camera) override;
	HRESULT GetCamera(IDirect3DRMFrame** camera) override;
	HRESULT SetProjection(D3DRMPROJECTIONTYPE type) override;
	D3DRMPROJECTIONTYPE GetProjection() override;
	HRESULT SetFront(D3DVALUE z) override;
	D3DVALUE GetFront() override;
	HRESULT SetBack(D3DVALUE z) override;
	D3DVALUE GetBack() override;
	HRESULT SetField(D3DVALUE field) override;
	D3DVALUE GetField() override;
	DWORD GetWidth() override;
	DWORD GetHeight() override;
	HRESULT Transform(D3DRMVECTOR4D* screen, D3DVECTOR* world) override;
	HRESULT InverseTransform(D3DVECTOR* world, D3DRMVECTOR4D* screen) override;
	HRESULT Pick(float x, float y, LPDIRECT3DRMPICKEDARRAY* pickedArray) override;
	void CloseDevice();
	HRESULT CollectSceneData();
	void PushVertices(const PositionColorVertex* vertices, size_t count);

private:
	void FreeDeviceResources();
	int m_vertexBufferCount = 0;
	int m_vertexCount;
	D3DCOLOR m_backgroundColor = 0xFF000000;
	DWORD m_width;
	DWORD m_height;
	IDirect3DRMFrame* m_rootFrame = nullptr;
	IDirect3DRMFrame* m_camera = nullptr;
	SDL_GPUDevice* m_device;
	SDL_GPUGraphicsPipeline* m_pipeline;
	SDL_GPUTexture* m_transferTexture;
	SDL_GPUTexture* m_depthTexture;
	SDL_GPUTransferBuffer* m_downloadTransferBuffer;
	SDL_GPUBuffer* m_vertexBuffer = nullptr;
	SDL_Surface* m_renderedImage = nullptr;
	D3DVALUE m_front = 1.f;
	D3DVALUE m_back = 10.f;
	D3DVALUE m_field = 0.5f;

	ViewportUniforms m_uniforms;
};

struct Direct3DRMViewportArray_SDL3GPUImpl : public Direct3DRMArrayBase_SDL3GPUImpl<
												 IDirect3DRMViewport,
												 Direct3DRMViewport_SDL3GPUImpl,
												 IDirect3DRMViewportArray> {
	using Direct3DRMArrayBase_SDL3GPUImpl::Direct3DRMArrayBase_SDL3GPUImpl;
};
