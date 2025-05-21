
#include "miniwin_d3d.h"
#include "miniwin_ddpalette_p.h"
#include "miniwin_ddraw_p.h"
#include "miniwin_ddsurface_p.h"
#include "miniwin_p.h"

#include <SDL3/SDL.h>
#include <assert.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>

SDL_Window* DDWindow;
SDL_Surface* DDBackBuffer;

HRESULT IDirectDrawClipper::SetHWnd(DWORD unnamedParam1, HWND hWnd)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT DirectDrawImpl::QueryInterface(const GUID& riid, void** ppvObject)
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
	MINIWIN_NOT_IMPLEMENTED();
	return E_NOINTERFACE;
}

// IDirectDraw interface
HRESULT DirectDrawImpl::CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER* lplpDDClipper, IUnknown* pUnkOuter)
{
	*lplpDDClipper = new IDirectDrawClipper;

	return DD_OK;
}

HRESULT DirectDrawImpl::CreatePalette(
	DDPixelCaps dwFlags,
	LPPALETTEENTRY lpColorTable,
	LPDIRECTDRAWPALETTE* lplpDDPalette,
	IUnknown* pUnkOuter
)
{
	if ((dwFlags & DDPCAPS_8BIT) != DDPCAPS_8BIT) {
		return DDERR_INVALIDPARAMS;
	}

	*lplpDDPalette = static_cast<LPDIRECTDRAWPALETTE>(new DirectDrawPaletteImpl(lpColorTable));
	return DD_OK;
}

HRESULT DirectDrawImpl::CreateSurface(
	LPDDSURFACEDESC lpDDSurfaceDesc,
	LPDIRECTDRAWSURFACE* lplpDDSurface,
	IUnknown* pUnkOuter
)
{
	SDL_PixelFormat format;
#ifdef MINIWIN_PIXELFORMAT
	format = MINIWIN_PIXELFORMAT;
#else
	format = SDL_PIXELFORMAT_RGBA8888;
#endif
	if ((lpDDSurfaceDesc->dwFlags & DDSD_PIXELFORMAT) == DDSD_PIXELFORMAT) {
		if ((lpDDSurfaceDesc->ddpfPixelFormat.dwFlags & DDPF_RGB) == DDPF_RGB) {
			switch (lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount) {
			case 8:
				format = SDL_PIXELFORMAT_INDEX8;
				break;
			case 16:
				format = SDL_PIXELFORMAT_RGB565;
				break;
			}
		}
	}
	if ((lpDDSurfaceDesc->dwFlags & DDSD_CAPS) == DDSD_CAPS) {
		if ((lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_ZBUFFER) == DDSCAPS_ZBUFFER) {
			if ((lpDDSurfaceDesc->dwFlags & DDSD_ZBUFFERBITDEPTH) != DDSD_ZBUFFERBITDEPTH) {
				return DDERR_INVALIDPARAMS;
			}
			SDL_Log("Todo: Set %dbit Z-Buffer", lpDDSurfaceDesc->dwZBufferBitDepth);
			*lplpDDSurface = static_cast<IDirectDrawSurface*>(new DirectDrawSurfaceImpl);
			return DD_OK;
		}
		if ((lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) == DDSCAPS_PRIMARYSURFACE) {
			SDL_Surface* windowSurface = SDL_GetWindowSurface(DDWindow);
			if (!windowSurface) {
				return DDERR_GENERIC;
			}
			int width, height;
			SDL_GetWindowSize(DDWindow, &width, &height);
			bool implicitFlip = (lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_FLIP) != DDSCAPS_FLIP;
			auto frontBuffer = new DirectDrawSurfaceImpl(width, height, windowSurface->format);
			frontBuffer->SetAutoFlip(implicitFlip);
			*lplpDDSurface = static_cast<IDirectDrawSurface*>(frontBuffer);
			return DD_OK;
		}
		if ((lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_OFFSCREENPLAIN) == DDSCAPS_OFFSCREENPLAIN) {
			MINIWIN_TRACE("DDSCAPS_OFFSCREENPLAIN"); // 2D surfaces?
		}
		if ((lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) == DDSCAPS_SYSTEMMEMORY) {
			MINIWIN_TRACE("DDSCAPS_SYSTEMMEMORY"); // Software rendering?
		}
		if ((lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_TEXTURE) == DDSCAPS_TEXTURE) {
			MINIWIN_TRACE("DDSCAPS_TEXTURE"); // Texture for use in 3D
		}
		if ((lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_3DDEVICE) == DDSCAPS_3DDEVICE) {
			MINIWIN_TRACE("DDSCAPS_3DDEVICE"); // back buffer
		}
		if ((lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY) == DDSCAPS_VIDEOMEMORY) {
			MINIWIN_TRACE("DDSCAPS_VIDEOMEMORY"); // front / back buffer
		}
	}

	if ((lpDDSurfaceDesc->dwFlags & (DDSD_WIDTH | DDSD_HEIGHT)) != (DDSD_WIDTH | DDSD_HEIGHT)) {
		return DDERR_INVALIDPARAMS;
	}

	int width = lpDDSurfaceDesc->dwWidth;
	int height = lpDDSurfaceDesc->dwHeight;
	if (width == 0 || height == 0) {
		return DDERR_INVALIDPARAMS;
	}
	*lplpDDSurface = static_cast<IDirectDrawSurface*>(new DirectDrawSurfaceImpl(width, height, format));
	return DD_OK;
}

HRESULT DirectDrawImpl::EnumDisplayModes(
	DWORD dwFlags,
	LPDDSURFACEDESC lpDDSurfaceDesc,
	LPVOID lpContext,
	LPDDENUMMODESCALLBACK lpEnumModesCallback
)
{
	SDL_DisplayID displayID = SDL_GetPrimaryDisplay();
	if (!displayID) {
		return DDERR_GENERIC;
	}

	int count_modes;
	SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(displayID, &count_modes);
	if (!modes) {
		return DDERR_GENERIC;
	}

	SDL_PixelFormat format;
	HRESULT status = S_OK;

	for (int i = 0; i < count_modes; i++) {
#ifdef MINIWIN_PIXELFORMAT
		format = MINIWIN_PIXELFORMAT;
#else
		format = modes[i]->format;
#endif

		const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(format);
		if (!details) {
			continue;
		}
		DDSURFACEDESC ddsd = {};
		ddsd.dwSize = sizeof(DDSURFACEDESC);
		ddsd.dwFlags = DDSD_WIDTH | DDSD_HEIGHT;
		ddsd.dwWidth = modes[i]->w;
		ddsd.dwHeight = modes[i]->h;
		ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
		ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB;
		ddsd.ddpfPixelFormat.dwRGBBitCount = details->bits_per_pixel;
		if (details->bits_per_pixel == 8) {
			ddsd.ddpfPixelFormat.dwFlags |= DDPF_PALETTEINDEXED8;
		}
		ddsd.ddpfPixelFormat.dwRBitMask = details->Rmask;
		ddsd.ddpfPixelFormat.dwGBitMask = details->Gmask;
		ddsd.ddpfPixelFormat.dwBBitMask = details->Bmask;

		if (!lpEnumModesCallback(&ddsd, lpContext)) {
			status = DDERR_GENERIC;
		}
	}
	SDL_free(modes);

	return status;
}

HRESULT DirectDrawImpl::FlipToGDISurface()
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT DirectDrawImpl::GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps)
{
	if (lpDDDriverCaps) {
		if (lpDDDriverCaps->dwSize >= sizeof(DDCAPS)) {
			lpDDDriverCaps->dwCaps2 = DDCAPS2_CERTIFIED; // Required to enable lighting
		}
	}

	if (lpDDHELCaps) {
		if (lpDDDriverCaps->dwSize >= sizeof(DDCAPS)) {
			lpDDDriverCaps->dwCaps2 = DDCAPS2_CERTIFIED; // Required to enable lighting
		}
	}

	return S_OK;
}

HRESULT DirectDrawImpl::EnumDevices(LPD3DENUMDEVICESCALLBACK cb, void* ctx)
{
	int numDrivers = SDL_GetNumRenderDrivers();
	if (numDrivers <= 0) {
		return DDERR_GENERIC;
	}

	DDBitDepths renderBitDepth = DDBD_32;

#ifdef MINIWIN_PIXELFORMAT
	SDL_PixelFormat format = MINIWIN_PIXELFORMAT;
	const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(format);
	switch (details->bits_per_pixel) {
	case 8:
		renderBitDepth = DDBD_8;
		break;
	case 16:
		renderBitDepth = DDBD_16;
		break;
	case 24:
		renderBitDepth = DDBD_24;
		break;
	default:
		break;
	}
#endif

	const char* deviceDesc = "SDL3 SDL_Renderer";

	for (int i = 0; i < numDrivers; ++i) {
		const char* deviceName = SDL_GetRenderDriver(i);
		if (!deviceName) {
			return DDERR_GENERIC;
		}

		GUID deviceGuid = {0x682656F3, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, (uint8_t) i}};

		D3DDEVICEDESC halDesc = {};
		halDesc.dcmColorModel = D3DCOLORMODEL::RGB;
		halDesc.dwFlags = D3DDD_DEVICEZBUFFERBITDEPTH;
		halDesc.dwDeviceZBufferBitDepth = DDBD_8 | DDBD_16 | DDBD_24 | DDBD_32;
		halDesc.dwDeviceRenderBitDepth = renderBitDepth;
		halDesc.dpcTriCaps.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE;
		halDesc.dpcTriCaps.dwShadeCaps = D3DPSHADECAPS_ALPHAFLATBLEND;
		halDesc.dpcTriCaps.dwTextureFilterCaps = D3DPTFILTERCAPS_LINEAR;

		char* deviceNameDup = SDL_strdup(deviceName);
		char* deviceDescDup = SDL_strdup(deviceDesc);
		cb(&deviceGuid, deviceNameDup, deviceDescDup, &halDesc, &halDesc, ctx);
		SDL_free(deviceDescDup);
		SDL_free(deviceNameDup);
	}

	return S_OK;
}

HRESULT DirectDrawImpl::GetDisplayMode(LPDDSURFACEDESC lpDDSurfaceDesc)
{
	SDL_DisplayID displayID = SDL_GetPrimaryDisplay();
	if (!displayID) {
		return DDERR_GENERIC;
	}

	const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(displayID);
	if (!mode) {
		return DDERR_GENERIC;
	}

	SDL_PixelFormat format;
#ifdef MINIWIN_PIXELFORMAT
	format = MINIWIN_PIXELFORMAT;
#else
	format = mode->format;
#endif

	const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(format);

	lpDDSurfaceDesc->dwFlags = DDSD_WIDTH | DDSD_HEIGHT;
	lpDDSurfaceDesc->dwWidth = mode->w;
	lpDDSurfaceDesc->dwHeight = mode->h;
	lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount = details->bits_per_pixel;
	lpDDSurfaceDesc->ddpfPixelFormat.dwFlags = DDPF_RGB;
	if (details->bits_per_pixel == 8) {
		lpDDSurfaceDesc->ddpfPixelFormat.dwFlags |= DDPF_PALETTEINDEXED8;
	}
	lpDDSurfaceDesc->ddpfPixelFormat.dwRBitMask = details->Rmask;
	lpDDSurfaceDesc->ddpfPixelFormat.dwGBitMask = details->Gmask;
	lpDDSurfaceDesc->ddpfPixelFormat.dwBBitMask = details->Bmask;

	return DD_OK;
}

HRESULT DirectDrawImpl::RestoreDisplayMode()
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT DirectDrawImpl::SetCooperativeLevel(HWND hWnd, DDSCLFlags dwFlags)
{
	if (hWnd) {
		bool fullscreen;
		if ((dwFlags & DDSCL_NORMAL) == DDSCL_NORMAL) {
			fullscreen = false;
		}
		else if ((dwFlags & DDSCL_FULLSCREEN) == DDSCL_FULLSCREEN) {
			fullscreen = true;
		}
		else {
			return DDERR_INVALIDPARAMS;
		}

		if (!SDL_SetWindowFullscreen(hWnd, fullscreen)) {
			return DDERR_GENERIC;
		}
		DDWindow = hWnd;
	}
	return DD_OK;
}

HRESULT DirectDrawImpl::SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

// IDirect3D2 interface
HRESULT DirectDrawImpl::CreateDevice(const GUID& guid, void* pBackBuffer, IDirect3DDevice2** ppDirect3DDevice)
{
	*ppDirect3DDevice = new IDirect3DDevice2;
	return DD_OK;
}

HRESULT DirectDrawCreate(LPGUID lpGuid, LPDIRECTDRAW* lplpDD, IUnknown* pUnkOuter)
{
	if (lpGuid) {
		MINIWIN_NOT_IMPLEMENTED();
	}

	*lplpDD = new DirectDrawImpl;

	return DD_OK;
}

HRESULT DirectDrawEnumerate(LPDDENUMCALLBACKA cb, void* context)
{
	int numDrivers = SDL_GetNumVideoDrivers();

	for (int i = 0; i < numDrivers; ++i) {
		const char* driverName = SDL_GetVideoDriver(i);
		char* driverNameDup = SDL_strdup(driverName);
		BOOL callback_result = cb(NULL, driverNameDup, NULL, context);
		SDL_free(driverNameDup);
		if (!callback_result) {
			return DDERR_GENERIC;
		}
	}

	return DD_OK;
}

UINT WINAPI GetSystemPaletteEntries(HDC hdc, UINT iStart, UINT cEntries, LPPALETTEENTRY pPalEntries)
{
	for (UINT i = 0; i < cEntries; i++) {
		UINT val = iStart + i;
		pPalEntries[i].peRed = val;
		pPalEntries[i].peGreen = val;
		pPalEntries[i].peBlue = val;
		pPalEntries[i].peFlags = PC_NONE;
	}
	return cEntries;
}

HPALETTE CreatePalette(LPLOGPALETTE lpLogPalette)
{
	MINIWIN_NOT_IMPLEMENTED();
	return nullptr;
}

int SelectPalette(HDC hdc, HPALETTE hpal, BOOL bForceBackground)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

int RealizePalette(HDC hdc)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}
