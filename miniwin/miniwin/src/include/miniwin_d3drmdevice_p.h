#pragma once

#include "miniwin_d3drm.h"
#include "miniwin_d3drmobject_p.h"

#include <SDL3/SDL.h>

struct Direct3DRMDevice2Impl : public Direct3DRMObjectBase<IDirect3DRMDevice2> {
	Direct3DRMDevice2Impl(DWORD width, DWORD height, SDL_GPUDevice* device);
	~Direct3DRMDevice2Impl() override;
	DWORD GetWidth() override;
	DWORD GetHeight() override;
	HRESULT SetBufferCount(int count) override;
	HRESULT GetBufferCount() override;
	HRESULT SetShades(DWORD shadeCount) override;
	HRESULT GetShades() override;
	HRESULT SetQuality(D3DRMRENDERQUALITY quality) override;
	D3DRMRENDERQUALITY GetQuality() override;
	HRESULT SetDither(int dither) override;
	HRESULT GetDither() override;
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
