#include "miniwin_ddraw.h"

#include "miniwin_d3d.h"

#include <SDL3/SDL.h>
#include <assert.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>

SDL_Renderer* renderer;

static SDL_FRect ConvertRect(const RECT* r)
{
	SDL_FRect sdlRect;
	sdlRect.x = r->left;
	sdlRect.y = r->top;
	sdlRect.w = r->right - r->left;
	sdlRect.h = r->bottom - r->top;
	return sdlRect;
}

struct DirectDrawSurfaceImpl : public IDirectDrawSurface3 {
	DirectDrawSurfaceImpl() {}
	DirectDrawSurfaceImpl(int width, int height)
	{
		texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, width, height);
		if (!texture) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create texture: %s", SDL_GetError());
		}
	}

	~DirectDrawSurfaceImpl() override
	{
		if (texture) {
			SDL_DestroyTexture(texture);
		}
	}
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
		DDBltFlags dwFlags,
		LPDDBLTFX lpDDBltFx
	) override
	{
		if (!renderer) {
			return DDERR_GENERIC;
		}
		SDL_FRect srcRect = ConvertRect(lpSrcRect);
		SDL_FRect dstRect = ConvertRect(lpDestRect);
		SDL_RenderTexture(renderer, static_cast<DirectDrawSurfaceImpl*>(lpDDSrcSurface)->texture, &srcRect, &dstRect);
		SDL_RenderPresent(renderer);
		return DD_OK;
	}
	HRESULT BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE lpDDSrcSurface, LPRECT lpSrcRect, DDBltFastFlags dwTrans)
		override
	{
		if (!renderer) {
			return DDERR_GENERIC;
		}
		SDL_FRect dstRect = {
			(float) dwX,
			(float) dwY,
			(float) (lpSrcRect->right - lpSrcRect->left),
			(float) (lpSrcRect->bottom - lpSrcRect->top)
		};
		SDL_FRect srcRect = ConvertRect(lpSrcRect);
		SDL_RenderTexture(renderer, static_cast<DirectDrawSurfaceImpl*>(lpDDSrcSurface)->texture, &srcRect, &dstRect);
		SDL_RenderPresent(renderer);
		return DD_OK;
	}
	HRESULT Flip(LPDIRECTDRAWSURFACE lpDDSurfaceTargetOverride, DDFlipFlags dwFlags) override
	{
		if (!renderer || !texture) {
			return DDERR_GENERIC;
		}
		float width, height;
		SDL_GetTextureSize(texture, &width, &height);
		SDL_FRect rect{0, 0, width, height};
		SDL_RenderTexture(renderer, texture, &rect, &rect);
		SDL_RenderPresent(renderer);
		return DD_OK;
	}
	HRESULT GetAttachedSurface(LPDDSCAPS lpDDSCaps, LPDIRECTDRAWSURFACE* lplpDDAttachedSurface) override
	{
		if ((lpDDSCaps->dwCaps & DDSCAPS_BACKBUFFER) != DDSCAPS_BACKBUFFER) {
			return DDERR_INVALIDPARAMS;
		}
		*lplpDDAttachedSurface = static_cast<IDirectDrawSurface*>(this);
		return DD_OK;
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
	HRESULT GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc) override
	{
		if (!texture) {
			return DDERR_GENERIC;
		}
		const SDL_PixelFormatDetails* format = SDL_GetPixelFormatDetails(texture->format);
		lpDDSurfaceDesc->ddpfPixelFormat.dwFlags = DDPF_RGB;
		lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount = (format->bits_per_pixel == 8) ? 8 : 16;
		lpDDSurfaceDesc->ddpfPixelFormat.dwRBitMask = format->Rmask;
		lpDDSurfaceDesc->ddpfPixelFormat.dwGBitMask = format->Gmask;
		lpDDSurfaceDesc->ddpfPixelFormat.dwBBitMask = format->Bmask;
		return DD_OK;
	}
	HRESULT IsLost() override { return DD_OK; }
	HRESULT Lock(LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DDLockFlags dwFlags, HANDLE hEvent) override
	{
		if (!lpDDSurfaceDesc) {
			return DDERR_INVALIDPARAMS;
		}
		if (!texture) {
			return DDERR_GENERIC;
		}

		int pitch = 0;
		void* pixels = nullptr;
		if (SDL_LockTexture(texture, (SDL_Rect*) lpDestRect, &pixels, &pitch) < 0) {
			return DDERR_GENERIC;
		}

		lpDDSurfaceDesc->lpSurface = pixels;
		lpDDSurfaceDesc->lPitch = pitch;
		const SDL_PixelFormatDetails* format = SDL_GetPixelFormatDetails(texture->format);
		lpDDSurfaceDesc->ddpfPixelFormat.dwFlags = DDPF_RGB;
		lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount = (format->bits_per_pixel == 8) ? 8 : 16;
		lpDDSurfaceDesc->ddpfPixelFormat.dwRBitMask = format->Rmask;
		lpDDSurfaceDesc->ddpfPixelFormat.dwGBitMask = format->Gmask;
		lpDDSurfaceDesc->ddpfPixelFormat.dwBBitMask = format->Bmask;

		return DD_OK;
	}
	HRESULT ReleaseDC(HDC hDC) override { return DD_OK; }
	HRESULT Restore() override { return DD_OK; }
	HRESULT SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper) override { return DD_OK; }
	HRESULT SetColorKey(DDColorKeyFlags dwFlags, LPDDCOLORKEY lpDDColorKey) override { return DD_OK; }
	HRESULT SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) override { return DD_OK; }
	HRESULT Unlock(LPVOID lpSurfaceData) override
	{
		if (texture) {
			SDL_UnlockTexture(texture);
			return DD_OK;
		}
		return DDERR_GENERIC;
	}

private:
	SDL_Texture* texture = nullptr;
};

struct DirectDrawClipperImpl : public IDirectDrawClipper {
	// IDirectDrawClipper interface
	HRESULT SetHWnd(DWORD unnamedParam1, HWND hWnd) override { return DD_OK; }
};

struct DirectDrawPaletteImpl : public IDirectDrawPalette {
	HRESULT GetCaps(LPDWORD lpdwCaps) override { return DD_OK; }
	HRESULT GetEntries(DWORD dwFlags, DWORD dwBase, DWORD dwNumEntries, LPPALETTEENTRY lpEntries) override
	{
		return DD_OK;
	}
	HRESULT SetEntries(DWORD dwFlags, DWORD dwStartingEntry, DWORD dwCount, LPPALETTEENTRY lpEntries) override
	{
		return DD_OK;
	}
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
		DDPixelCaps dwFlags,
		LPPALETTEENTRY lpColorTable,
		LPDIRECTDRAWPALETTE* lplpDDPalette,
		IUnknown* pUnkOuter
	) override
	{
		*lplpDDPalette = static_cast<LPDIRECTDRAWPALETTE>(new DirectDrawPaletteImpl);
		return DD_OK;
	}
	HRESULT CreateSurface(LPDDSURFACEDESC lpDDSurfaceDesc, LPDIRECTDRAWSURFACE* lplpDDSurface, IUnknown* pUnkOuter)
		override
	{
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
				if ((lpDDSurfaceDesc->dwFlags & DDSD_BACKBUFFERCOUNT) == DDSD_BACKBUFFERCOUNT) {
					SDL_Log("Todo: Switch to %d buffering", lpDDSurfaceDesc->dwBackBufferCount);
				}
				int width, height;
				SDL_GetRenderOutputSize(renderer, &width, &height);
				*lplpDDSurface = static_cast<IDirectDrawSurface*>(new DirectDrawSurfaceImpl(width, height));
				return DD_OK;
			}
			if ((lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_OFFSCREENPLAIN) == DDSCAPS_OFFSCREENPLAIN) {
				SDL_Log("DDSCAPS_OFFSCREENPLAIN"); // 2D surfaces?
			}
			if ((lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) == DDSCAPS_SYSTEMMEMORY) {
				SDL_Log("DDSCAPS_SYSTEMMEMORY"); // Software rendering?
			}
			if ((lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_TEXTURE) == DDSCAPS_TEXTURE) {
				SDL_Log("DDSCAPS_TEXTURE"); // Texture for use in 3D
			}
			if ((lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_3DDEVICE) == DDSCAPS_3DDEVICE) {
				SDL_Log("DDSCAPS_3DDEVICE"); // back buffer
			}
			if ((lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY) == DDSCAPS_VIDEOMEMORY) {
				SDL_Log("DDSCAPS_VIDEOMEMORY"); // front / back buffer
			}
		}

		if ((lpDDSurfaceDesc->dwFlags & DDSD_PIXELFORMAT) == DDSD_PIXELFORMAT) {
			if ((lpDDSurfaceDesc->ddpfPixelFormat.dwFlags & DDPF_RGB) == DDPF_RGB) {
				SDL_Log("DDPF_RGB"); // Use dwRGBBitCount to choose the texture format
			}
		}

		if ((lpDDSurfaceDesc->dwFlags & (DDSD_WIDTH | DDSD_HEIGHT)) != (DDSD_WIDTH | DDSD_HEIGHT)) {
			return DDERR_INVALIDPARAMS;
		}

		int width = lpDDSurfaceDesc->dwWidth;
		int height = lpDDSurfaceDesc->dwHeight;
		*lplpDDSurface = static_cast<IDirectDrawSurface*>(new DirectDrawSurfaceImpl(width, height));
		return DD_OK;
	}
	HRESULT EnumDisplayModes(
		DWORD dwFlags,
		LPDDSURFACEDESC lpDDSurfaceDesc,
		LPVOID lpContext,
		LPDDENUMMODESCALLBACK lpEnumModesCallback
	) override;
	HRESULT FlipToGDISurface() override { return DD_OK; }
	HRESULT GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps) override;
	HRESULT GetDisplayMode(LPDDSURFACEDESC lpDDSurfaceDesc) override
	{
		SDL_DisplayID displayID = SDL_GetPrimaryDisplay();
		if (!displayID) {
			return DDERR_GENERIC;
		}

		const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(displayID);
		if (!mode) {
			return DDERR_GENERIC;
		}

		const SDL_PixelFormatDetails* format = SDL_GetPixelFormatDetails(mode->format);

		lpDDSurfaceDesc->dwFlags = DDSD_WIDTH | DDSD_HEIGHT;
		lpDDSurfaceDesc->dwWidth = mode->w;
		lpDDSurfaceDesc->dwHeight = mode->h;
		lpDDSurfaceDesc->ddpfPixelFormat.dwFlags = DDPF_RGB;
		lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount = (format->bits_per_pixel == 8) ? 8 : 16;
		lpDDSurfaceDesc->ddpfPixelFormat.dwRBitMask = format->Rmask;
		lpDDSurfaceDesc->ddpfPixelFormat.dwGBitMask = format->Gmask;
		lpDDSurfaceDesc->ddpfPixelFormat.dwBBitMask = format->Bmask;

		return DD_OK;
	}
	HRESULT RestoreDisplayMode() override { return DD_OK; }
	HRESULT SetCooperativeLevel(HWND hWnd, DDSCLFlags dwFlags) override
	{
		if (hWnd) {
			renderer = SDL_CreateRenderer(hWnd, NULL);
		}
		return DD_OK;
	}
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

	int numDrivers = SDL_GetNumRenderDrivers();
	if (numDrivers <= 0) {
		return DDERR_GENERIC;
	}

	const char* deviceDesc = "SDL3-backed renderer";
	char* deviceDescDup = SDL_strdup(deviceDesc);

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
		halDesc.dwDeviceRenderBitDepth = DDBD_8 | DDBD_16;
		halDesc.dpcTriCaps.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE;
		halDesc.dpcTriCaps.dwShadeCaps = D3DPSHADECAPS_ALPHAFLATBLEND;
		halDesc.dpcTriCaps.dwTextureFilterCaps = D3DPTFILTERCAPS_LINEAR;

		char* deviceNameDup = SDL_strdup(deviceName);
		cb(&deviceGuid, deviceNameDup, deviceDescDup, &halDesc, &halDesc, ctx);
		SDL_free(deviceNameDup);
	}
	SDL_free(deviceDescDup);

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
