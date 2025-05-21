#pragma once

#include "miniwin_d3drm.h"
#include "miniwin_d3drmdevice_p.h"
#include "miniwin_d3drmobject_p.h"

#include <SDL3/SDL.h>

struct Direct3DRMViewportImpl : public Direct3DRMObjectBase<IDirect3DRMViewport> {
	Direct3DRMViewportImpl(
		DWORD width,
		DWORD height,
		SDL_GPUDevice* device,
		SDL_GPUTexture* transferTexture,
		SDL_GPUTransferBuffer* downloadTransferBuffer,
		SDL_GPUGraphicsPipeline* pipeline
	);
	~Direct3DRMViewportImpl() override;
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
	void Update();

private:
	void FreeDeviceResources();
	int m_vertexCount;
	bool m_updated = false;
	DWORD m_width;
	DWORD m_height;
	IDirect3DRMFrame* m_camera = nullptr;
	SDL_GPUDevice* m_device;
	SDL_GPUGraphicsPipeline* m_pipeline;
	SDL_GPUTexture* m_transferTexture;
	SDL_GPUTransferBuffer* m_downloadTransferBuffer;
	SDL_GPUBuffer* m_vertexBuffer;
	SDL_Surface* m_renderedImage = nullptr;
};
