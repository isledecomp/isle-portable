#pragma once

#include "d3drmobject_impl.h"
#include "d3drmrenderer.h"
#include "miniwin/d3drm.h"
#include "miniwin/miniwindevice.h"

#include <SDL3/SDL.h>

struct Direct3DRMDevice2Impl : public Direct3DRMObjectBaseImpl<IDirect3DRMDevice2>, public IDirect3DRMMiniwinDevice {
	Direct3DRMDevice2Impl(DWORD width, DWORD height, Direct3DRMRenderer* renderer);
	~Direct3DRMDevice2Impl() override;
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override;
	DWORD GetWidth() override;
	DWORD GetHeight() override;
	HRESULT SetBufferCount(int count) override;
	DWORD GetBufferCount() override;
	HRESULT SetShades(DWORD shadeCount) override;
	DWORD GetShades() override;
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

	// IDirect3DRMMiniwinDevice interface
	bool ConvertEventToRenderCoordinates(SDL_Event* event) override;
	float GetShininessFactor() override;
	HRESULT SetShininessFactor(float factor) override;

	Direct3DRMRenderer* m_renderer;

private:
	DWORD m_width;
	DWORD m_height;
	IDirect3DRMViewportArray* m_viewports;
};
