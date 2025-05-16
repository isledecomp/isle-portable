#include "miniwin_ddraw_p.h"
#include "miniwin_ddsurface_p.h"
#include "miniwin_p.h"

#include <assert.h>

DirectDrawSurfaceImpl::DirectDrawSurfaceImpl()
{
}

DirectDrawSurfaceImpl::DirectDrawSurfaceImpl(int width, int height)
{
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, width, height);
	if (!texture) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create texture: %s", SDL_GetError());
	}
}

DirectDrawSurfaceImpl::~DirectDrawSurfaceImpl()
{
	if (texture) {
		SDL_DestroyTexture(texture);
	}
}

// IUnknown interface
HRESULT DirectDrawSurfaceImpl::QueryInterface(const GUID& riid, void** ppvObject)
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
HRESULT DirectDrawSurfaceImpl::AddAttachedSurface(LPDIRECTDRAWSURFACE lpDDSAttachedSurface)
{
	return DD_OK;
}
HRESULT DirectDrawSurfaceImpl::Blt(
	LPRECT lpDestRect,
	LPDIRECTDRAWSURFACE lpDDSrcSurface,
	LPRECT lpSrcRect,
	DDBltFlags dwFlags,
	LPDDBLTFX lpDDBltFx
)
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

HRESULT DirectDrawSurfaceImpl::BltFast(
	DWORD dwX,
	DWORD dwY,
	LPDIRECTDRAWSURFACE lpDDSrcSurface,
	LPRECT lpSrcRect,
	DDBltFastFlags dwTrans
)
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

HRESULT DirectDrawSurfaceImpl::Flip(LPDIRECTDRAWSURFACE lpDDSurfaceTargetOverride, DDFlipFlags dwFlags)
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

HRESULT DirectDrawSurfaceImpl::GetAttachedSurface(LPDDSCAPS lpDDSCaps, LPDIRECTDRAWSURFACE* lplpDDAttachedSurface)
{
	if ((lpDDSCaps->dwCaps & DDSCAPS_BACKBUFFER) != DDSCAPS_BACKBUFFER) {
		return DDERR_INVALIDPARAMS;
	}
	*lplpDDAttachedSurface = static_cast<IDirectDrawSurface*>(this);
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::GetCaps(LPDDSCAPS lpDDSCaps)
{
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::GetDC(HDC* lphDC)
{
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::GetOverlayPosition(LPLONG lplX, LPLONG lplY)
{
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::GetPalette(LPDIRECTDRAWPALETTE* lplpDDPalette)
{
	assert(false && "unimplemented");
	return DDERR_GENERIC;
}

HRESULT DirectDrawSurfaceImpl::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat)
{
	memset(lpDDPixelFormat, 0, sizeof(*lpDDPixelFormat));
	lpDDPixelFormat->dwFlags = DDPF_RGB;
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc)
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

HRESULT DirectDrawSurfaceImpl::IsLost()
{
	return DD_OK;
}
HRESULT DirectDrawSurfaceImpl::Lock(
	LPRECT lpDestRect,
	LPDDSURFACEDESC lpDDSurfaceDesc,
	DDLockFlags dwFlags,
	HANDLE hEvent
)
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

HRESULT DirectDrawSurfaceImpl::ReleaseDC(HDC hDC)
{
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::Restore()
{
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper)
{
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::SetColorKey(DDColorKeyFlags dwFlags, LPDDCOLORKEY lpDDColorKey)
{
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette)
{
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::Unlock(LPVOID lpSurfaceData)
{
	if (!texture) {
		return DDERR_GENERIC;
	}
	SDL_UnlockTexture(texture);
	return DD_OK;
}
