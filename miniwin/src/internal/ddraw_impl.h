#pragma once

#include "d3drmrenderer.h"
#include "framebuffer_impl.h"
#include "miniwin/d3d.h"
#include "miniwin/ddraw.h"
#include "miniwin/miniwind3d.h"

#include <SDL3/SDL.h>

extern SDL_Window* DDWindow;
extern Direct3DRMRenderer* DDRenderer;

inline static SDL_Rect ConvertRect(const RECT* r)
{
	return {r->left, r->top, r->right - r->left, r->bottom - r->top};
}

struct DirectDrawImpl : public IDirectDraw2, public IDirect3D2, public IDirect3DMiniwin {
	// IUnknown interface
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override;
	// IDirectDraw interface
	HRESULT CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER* lplpDDClipper, IUnknown* pUnkOuter) override;
	HRESULT
	CreatePalette(
		DDPixelCaps dwFlags,
		LPPALETTEENTRY lpColorTable,
		LPDIRECTDRAWPALETTE* lplpDDPalette,
		IUnknown* pUnkOuter
	) override;
	HRESULT CreateSurface(LPDDSURFACEDESC lpDDSurfaceDesc, LPDIRECTDRAWSURFACE* lplpDDSurface, IUnknown* pUnkOuter)
		override;
	HRESULT EnumDisplayModes(
		DWORD dwFlags,
		LPDDSURFACEDESC lpDDSurfaceDesc,
		LPVOID lpContext,
		LPDDENUMMODESCALLBACK lpEnumModesCallback
	) override;
	HRESULT FlipToGDISurface() override;
	HRESULT GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps) override;
	HRESULT GetDisplayMode(LPDDSURFACEDESC lpDDSurfaceDesc) override;
	HRESULT RestoreDisplayMode() override;
	HRESULT SetCooperativeLevel(HWND hWnd, DDSCLFlags dwFlags) override;
	HRESULT SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP) override;
	// IDirect3D2 interface
	HRESULT CreateDevice(const GUID& guid, IDirectDrawSurface* pBackBuffer, IDirect3DDevice2** ppDirect3DDevice)
		override;
	HRESULT EnumDevices(LPD3DENUMDEVICESCALLBACK cb, void* ctx) override;
	// IDirect3DMiniwin interface
	HRESULT RequestMSAA(DWORD msaaSamples) override
	{
		m_msaaSamples = msaaSamples;
		return DD_OK;
	}
	DWORD GetMSAASamples() const override { return m_msaaSamples; }
	HRESULT RequestAnisotropic(float anisotropic) override
	{
		m_anisotropic = anisotropic;
		return DD_OK;
	}
	float GetAnisotropic() const override { return m_anisotropic; }

private:
	FrameBufferImpl* m_frameBuffer;
	int m_virtualWidth = 0;
	int m_virtualHeight = 0;
	DWORD m_msaaSamples = 0;
	float m_anisotropic = 0.0f;
};

HRESULT DirectDrawEnumerate(LPDDENUMCALLBACKA cb, void* context);

HRESULT DirectDrawCreate(LPGUID lpGuid, LPDIRECTDRAW* lplpDD, IUnknown* pUnkOuter);

void EnumDevice(
	LPD3DENUMDEVICESCALLBACK cb,
	void* ctx,
	const char* name,
	D3DDEVICEDESC* halDesc,
	D3DDEVICEDESC* helDesc,
	GUID deviceGuid
);
