#include "miniwin_ddraw.h"

#include "miniwin_d3d.h"

#include <SDL3/SDL.h>

struct DirectDrawSurfaceImpl : public IDirectDrawSurface3 {
	// IUnknown interface
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override
	{
		if (SDL_memcmp(&riid, &IID_IDirectDrawSurface3, sizeof(GUID)) == 0) {
			this->IUnknown::AddRef();
			*ppvObject = static_cast<IDirectDrawSurface3*>(this);
			return S_OK;
		}
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DirectDrawImpl does not implement guid");
		return E_NOINTERFACE;
	}
	// IDirectDrawSurface interface
	HRESULT AddAttachedSurface(LPDIRECTDRAWSURFACE lpDDSAttachedSurface) override { return DD_OK; }
	HRESULT Blt(
		LPRECT lpDestRect,
		LPDIRECTDRAWSURFACE lpDDSrcSurface,
		LPRECT lpSrcRect,
		DWORD dwFlags,
		LPDDBLTFX lpDDBltFx
	) override
	{
		return DD_OK;
	}
	HRESULT BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans) override
	{
		return DD_OK;
	}
	HRESULT DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDSAttachedSurface) override { return DD_OK; }
	HRESULT Flip(LPDIRECTDRAWSURFACE lpDDSurfaceTargetOverride, DWORD dwFlags) override { return DDERR_GENERIC; }
	HRESULT GetAttachedSurface(LPDDSCAPS lpDDSCaps, LPDIRECTDRAWSURFACE* lplpDDAttachedSurface) override
	{
		return DDERR_GENERIC;
	}
	HRESULT GetCaps(LPDDSCAPS lpDDSCaps) override { return DD_OK; }
	HRESULT GetDC(HDC* lphDC) override { return DD_OK; }
	HRESULT GetOverlayPosition(LPLONG lplX, LPLONG lplY) override { return DD_OK; }
	HRESULT GetPalette(LPDIRECTDRAWPALETTE* lplpDDPalette) override { return DDERR_GENERIC; }
	HRESULT GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) override
	{
		memset(lpDDPixelFormat, 0, sizeof(*lpDDPixelFormat));
		lpDDPixelFormat->dwFlags = DDPF_RGB;
		return DD_OK;
	}
	HRESULT GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc) override { return DD_OK; }
	HRESULT Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC lpDDSurfaceDesc) override { return DD_OK; }
	HRESULT IsLost() override { return DD_OK; }
	HRESULT Lock(LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent) override
	{
		return DD_OK;
	}
	HRESULT ReleaseDC(HDC hDC) override { return DD_OK; }
	HRESULT Restore() override { return DD_OK; }
	HRESULT SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper) override { return DD_OK; }
	HRESULT SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) override { return DD_OK; }
	HRESULT SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) override { return DD_OK; }
	HRESULT Unlock(LPVOID lpSurfaceData) override { return DD_OK; }
};

struct DirectDrawClipperImpl : public IDirectDrawClipper {
	// IDirectDrawClipper interface
	HRESULT SetHWnd(DWORD unnamedParam1, HWND hWnd) override { return DD_OK; }
};

struct DirectDrawImpl : public IDirectDraw2, public IDirect3D2 {
	// IUnknown interface
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override
	{
		if (SDL_memcmp(&riid, &IID_IDirectDraw2, sizeof(GUID)) == 0) {
			this->IUnknown::AddRef();
			*ppvObject = static_cast<IDirectDraw2*>(this);
			return S_OK;
		}
		if (SDL_memcmp(&riid, &IID_IDirect3D2, sizeof(GUID)) == 0) {
			this->IUnknown::AddRef();
			*ppvObject = static_cast<IDirect3D2*>(this);
			return S_OK;
		}
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DirectDrawImpl does not implement guid");
		return E_NOINTERFACE;
	}
	// IDirecdtDraw interface
	HRESULT CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER* lplpDDClipper, IUnknown* pUnkOuter) override
	{
		*lplpDDClipper = static_cast<IDirectDrawClipper*>(new DirectDrawClipperImpl);

		return DD_OK;
	}
	HRESULT CreatePalette(
		DWORD dwFlags,
		LPPALETTEENTRY lpColorTable,
		LPDIRECTDRAWPALETTE* lplpDDPalette,
		IUnknown* pUnkOuter
	) override
	{
		return DDERR_GENERIC;
	}
	HRESULT CreateSurface(LPDDSURFACEDESC lpDDSurfaceDesc, LPDIRECTDRAWSURFACE* lplpDDSurface, IUnknown* pUnkOuter)
		override
	{
		*lplpDDSurface = static_cast<IDirectDrawSurface*>(new DirectDrawSurfaceImpl);

		return DD_OK;
	}
	HRESULT EnumDisplayModes(
		DWORD dwFlags,
		LPDDSURFACEDESC lpDDSurfaceDesc,
		LPVOID lpContext,
		LPDDENUMMODESCALLBACK lpEnumModesCallback
	) override;
	HRESULT EnumSurfaces(
		DWORD dwFlags,
		LPDDSURFACEDESC lpDDSD,
		LPVOID lpContext,
		LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback
	) override
	{
		return DD_OK;
	}
	HRESULT FlipToGDISurface() override { return DD_OK; }
	HRESULT GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps) override;
	HRESULT GetDisplayMode(LPDDSURFACEDESC lpDDSurfaceDesc) override { return DD_OK; }
	HRESULT Initialize(GUID* lpGUID) override { return DD_OK; }
	HRESULT RestoreDisplayMode() override { return DD_OK; }
	HRESULT SetCooperativeLevel(HWND hWnd, DWORD dwFlags) override { return DD_OK; }
	HRESULT SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP) override { return DD_OK; }
	// IDirect3D2 interface
	HRESULT CreateDevice(const GUID& guid, void* pBackBuffer, IDirect3DDevice2** ppDirect3DDevice) override
	{
		*ppDirect3DDevice = new IDirect3DDevice2;
		return DD_OK;
	}
	HRESULT EnumDevices(LPD3DENUMDEVICESCALLBACK cb, void* ctx) override;
};

HRESULT DirectDrawImpl::EnumDevices(LPD3DENUMDEVICESCALLBACK cb, void* ctx)
{
	if (!cb) {
		return DDERR_INVALIDPARAMS;
	}

	GUID deviceGuid = {0xa4665c, 0x2673, 0x11ce, 0x8034a0};

	char* deviceName = (char*) "MiniWin 3D Device";
	char* deviceDesc = (char*) "Stubbed 3D device";

	D3DDEVICEDESC halDesc = {};
	halDesc.dcmColorModel = D3DCOLOR_RGB;
	halDesc.dwDeviceZBufferBitDepth = DDBD_16;
	halDesc.dpcTriCaps.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE;
	D3DDEVICEDESC helDesc = {};
	halDesc.dcmColorModel = D3DCOLOR_RGB;
	halDesc.dwDeviceZBufferBitDepth = DDBD_16;
	halDesc.dpcTriCaps.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE;

	cb(&deviceGuid, deviceName, deviceDesc, &halDesc, &helDesc, ctx);

	return S_OK;
}

HRESULT DirectDrawImpl::EnumDisplayModes(
	DWORD dwFlags,
	LPDDSURFACEDESC lpDDSurfaceDesc,
	LPVOID lpContext,
	LPDDENUMMODESCALLBACK lpEnumModesCallback
)
{
	if (!lpEnumModesCallback) {
		return DDERR_INVALIDPARAMS;
	}

	SDL_DisplayID displayID = SDL_GetPrimaryDisplay();
	if (!displayID) {
		return DDERR_GENERIC;
	}

	int count_modes;
	SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(displayID, &count_modes);
	if (!modes) {
		return DDERR_GENERIC;
	}

	HRESULT status = S_OK;

	for (int i = 0; i < count_modes; i++) {
		const SDL_PixelFormatDetails* format = SDL_GetPixelFormatDetails(modes[i]->format);
		if (!format) {
			continue;
		}
		DDSURFACEDESC ddsd = {};
		ddsd.dwSize = sizeof(DDSURFACEDESC);
		ddsd.dwFlags = DDSD_WIDTH | DDSD_HEIGHT;
		ddsd.dwWidth = modes[i]->w;
		ddsd.dwHeight = modes[i]->h;
		ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
		ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB;
		ddsd.ddpfPixelFormat.dwRGBBitCount =
			(format->bits_per_pixel == 8) ? 8 : 16; // Game only accepts 8 or 16 bit mode
		ddsd.ddpfPixelFormat.dwRBitMask = format->Rmask;
		ddsd.ddpfPixelFormat.dwGBitMask = format->Gmask;
		ddsd.ddpfPixelFormat.dwBBitMask = format->Bmask;

		if (!lpEnumModesCallback(&ddsd, lpContext)) {
			status = DDERR_GENERIC;
		}
	}
	SDL_free(modes);

	return status;
}

HRESULT DirectDrawImpl::GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps)
{
	if (lpDDDriverCaps) {
		memset(lpDDDriverCaps, 0, sizeof(DDCAPS));
		lpDDDriverCaps->dwSize = sizeof(DDCAPS);
		lpDDDriverCaps->dwCaps2 = DDCAPS2_CERTIFIED; // Required to enable lighting
	}

	if (lpDDHELCaps) {
		memset(lpDDHELCaps, 0, sizeof(DDCAPS));
		lpDDHELCaps->dwSize = sizeof(DDCAPS);
		lpDDHELCaps->dwCaps2 = DDCAPS2_CERTIFIED; // Required to enable lighting
	}

	return S_OK;
}

HRESULT DirectDrawCreate(LPGUID lpGuid, LPDIRECTDRAW* lplpDD, IUnknown* pUnkOuter)
{
	if (!lplpDD) {
		return DDERR_INVALIDPARAMS;
	}

	*lplpDD = new DirectDrawImpl;

	return DD_OK;
}

HRESULT DirectDrawEnumerate(LPDDENUMCALLBACKA cb, void* context)
{
	int numDrivers = SDL_GetNumVideoDrivers();

	for (int i = 0; i < numDrivers; ++i) {
		const char* driverName = SDL_GetVideoDriver(i);

		if (!cb(NULL, (LPSTR) driverName, NULL, context)) {
			return DDERR_GENERIC;
		}
	}

	return DD_OK;
}
