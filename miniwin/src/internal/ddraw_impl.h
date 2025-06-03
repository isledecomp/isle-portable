#pragma once

#include "d3drmrenderer.h"
#include "miniwin/d3d.h"
#include "miniwin/ddraw.h"

#include <SDL3/SDL.h>

extern SDL_Window* DDWindow;
extern SDL_Surface* DDBackBuffer;

struct DirectDrawImpl : public IDirectDraw2, public IDirect3D2 {
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
};

HRESULT DirectDrawEnumerate(LPDDENUMCALLBACKA cb, void* context);

HRESULT DirectDrawCreate(LPGUID lpGuid, LPDIRECTDRAW* lplpDD, IUnknown* pUnkOuter);

void EnumDevice(LPD3DENUMDEVICESCALLBACK cb, void* ctx, Direct3DRMRenderer* device, GUID deviceGuid);
