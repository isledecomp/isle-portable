#pragma once

#include "miniwin_d3drm.h"
#include "miniwin_d3drmobject_sdl3gpu.h"

#include <SDL3/SDL.h>

struct Direct3DRMDevice2_SDL3GPUImpl : public Direct3DRMObjectBase_SDL3GPUImpl<IDirect3DRMDevice2> {
	Direct3DRMDevice2_SDL3GPUImpl(DWORD width, DWORD height, SDL_GPUDevice* device);
	~Direct3DRMDevice2_SDL3GPUImpl() override;
	DWORD GetWidth() override;
	DWORD GetHeight() override;
	HRESULT SetBufferCount(int count) override;
	DWORD GetBufferCount() override;
	HRESULT SetShades(DWORD shadeCount) override;
	HRESULT GetShades() override;
	HRESULT SetQuality(D3DRMRENDERQUALITY quality) override;
	D3DRMRENDERQUALITY GetQuality() override;
	HRESULT SetDither(BOOL dither) override;
	BOOL GetDither() override;
	HRESULT SetTextureQuality(D3DRMTEXTUREQUALITY quality) override;
	D3DRMTEXTUREQUALITY GetTextureQuality() override;
	HRESULT SetRenderMode(D3DRMRENDERMODE mode) override;
	D3DRMRENDERMODE GetRenderMode() override;
	/**
	 * @brief Recalculating light positions, animation updates, etc.
	 */
	HRESULT Update() override;
	HRESULT AddViewport(IDirect3DRMViewport* viewport) override;
	HRESULT GetViewports(IDirect3DRMViewportArray** ppViewportArray) override;

	SDL_GPUDevice* m_device;

private:
	DWORD m_width;
	DWORD m_height;
	IDirect3DRMViewportArray* m_viewports;
};
