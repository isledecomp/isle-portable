#include "miniwin_ddraw.h"

#include <SDL3/SDL.h>

HRESULT IDirectDraw::EnumDisplayModes(
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

HRESULT IDirectDraw::GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps)
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

	*lplpDD = new IDirectDraw();

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
