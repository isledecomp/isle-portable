#include "miniwin_ddraw.h"

#include "miniwin_d3d.h"

#include <SDL3/SDL.h>

struct DirectDrawImpl : public IDirectDraw2, public IDirect3D2 {
	DirectDrawImpl() : m_refCount(1) {}
	// IUnknown interface
	ULONG AddRef() override
	{
		m_refCount += 1;
		return m_refCount;
	}
	ULONG Release() override
	{
		m_refCount -= 1;
		if (m_refCount == 0) {
			delete this;
		}
		return m_refCount;
	}
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override
	{
		if (SDL_memcmp(&riid, &IID_IDirectDraw2, sizeof(GUID)) == 0) {
			AddRef();
			*ppvObject = static_cast<IDirectDraw2*>(this);
			return S_OK;
		}
		if (SDL_memcmp(&riid, &IID_IDirect3D2, sizeof(GUID)) == 0) {
			AddRef();
			*ppvObject = static_cast<IDirect3D2*>(this);
			return S_OK;
		}
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DirectDrawImpl does not implement guid");
		return E_NOINTERFACE;
	}
	// IDirecdtDraw interface
	HRESULT CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER* lplpDDClipper, IUnknown* pUnkOuter) override
	{
		return DDERR_GENERIC;
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
		return DDERR_GENERIC;
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
		return DDERR_GENERIC;
	}
	HRESULT EnumDevices(LPD3DENUMDEVICESCALLBACK cb, void* ctx) override;

private:
	int m_refCount;
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
	DDSURFACEDESC ddsd = {};
	ddsd.dwSize = sizeof(DDSURFACEDESC);
	ddsd.dwFlags = DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
	ddsd.dwWidth = 640;
	ddsd.dwHeight = 480;
	ddsd.lPitch = 0;

	ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB | D3DDD_DEVICEZBUFFERBITDEPTH;
	ddsd.ddpfPixelFormat.dwRGBBitCount = 16; // Game only accpets 8 or 16

	if (!lpEnumModesCallback(&ddsd, lpContext)) {
		return DDERR_GENERIC;
	}

	return S_OK;
}

HRESULT DirectDrawImpl::GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps)
{
	if (lpDDDriverCaps) {
		memset(lpDDDriverCaps, 0, sizeof(DDCAPS));
		lpDDDriverCaps->dwSize = sizeof(DDCAPS);
		lpDDDriverCaps->dwCaps = 0;
	}

	if (lpDDHELCaps) {
		memset(lpDDHELCaps, 0, sizeof(DDCAPS));
		lpDDHELCaps->dwSize = sizeof(DDCAPS);
		lpDDHELCaps->dwCaps = 0;
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
