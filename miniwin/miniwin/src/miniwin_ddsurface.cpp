#include "miniwin_ddraw_p.h"
#include "miniwin_ddsurface_p.h"
#include "miniwin_p.h"

#include <assert.h>

DirectDrawSurfaceImpl::DirectDrawSurfaceImpl()
{
}

DirectDrawSurfaceImpl::DirectDrawSurfaceImpl(int width, int height)
{
	m_surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGB565);
	if (!m_surface) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create surface: %s", SDL_GetError());
	}
}

DirectDrawSurfaceImpl::~DirectDrawSurfaceImpl()
{
	if (m_surface) {
		SDL_DestroySurface(m_surface);
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
	auto srcSurface = static_cast<DirectDrawSurfaceImpl*>(lpDDSrcSurface);
	if (!srcSurface || !srcSurface->m_surface) {
		return DDERR_GENERIC;
	}

	SDL_Surface* windowSurface = SDL_GetWindowSurface(DDWindow);
	if (!windowSurface) {
		return DDERR_GENERIC;
	}

	SDL_Rect srcRect;
	if (lpSrcRect) {
		srcRect =
			{lpSrcRect->left, lpSrcRect->top, lpSrcRect->right - lpSrcRect->left, lpSrcRect->bottom - lpSrcRect->top};
	}
	else {
		srcRect = {0, 0, srcSurface->m_surface->w, srcSurface->m_surface->h};
	}

	SDL_Rect dstRect;
	if (lpDestRect) {
		dstRect = {
			lpDestRect->left,
			lpDestRect->top,
			lpDestRect->right - lpDestRect->left,
			lpDestRect->bottom - lpDestRect->top
		};
	}
	else {
		dstRect = {0, 0, m_surface->w, m_surface->h};
	}

	SDL_Surface* copy = SDL_ConvertSurface(srcSurface->m_surface, windowSurface->format);
	SDL_BlitSurface(copy, &srcRect, windowSurface, &dstRect);
	SDL_DestroySurface(copy);
	SDL_UpdateWindowSurface(DDWindow);
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
	SDL_Surface* windowSurface = SDL_GetWindowSurface(DDWindow);
	if (!windowSurface) {
		return DDERR_GENERIC;
	}
	auto srcSurface = static_cast<DirectDrawSurfaceImpl*>(lpDDSrcSurface);
	SDL_Rect srcRect;
	if (lpSrcRect) {
		srcRect = ConvertRect(lpSrcRect);
	}
	else {
		srcRect = {0, 0, srcSurface->m_surface->w, srcSurface->m_surface->h};
	}
	SDL_Rect dstRect = {(int) dwX, (int) dwY, srcRect.w, srcRect.h};
	bool SDL_BlitSurface(SDL_Surface * src, const SDL_Rect* srcrect, SDL_Surface* dst, const SDL_Rect* dstrect);
	SDL_Surface* copy = SDL_ConvertSurface(srcSurface->m_surface, windowSurface->format);
	SDL_BlitSurface(copy, &srcRect, windowSurface, &dstRect);
	SDL_DestroySurface(copy);
	SDL_UpdateWindowSurface(DDWindow);
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::Flip(LPDIRECTDRAWSURFACE lpDDSurfaceTargetOverride, DDFlipFlags dwFlags)
{
	if (!m_surface) {
		return DDERR_GENERIC;
	}
	SDL_Surface* windowSurface = SDL_GetWindowSurface(DDWindow);
	if (!windowSurface) {
		return DDERR_GENERIC;
	}
	SDL_Rect srcRect{0, 0, m_surface->w, m_surface->h};
	SDL_Surface* copy = SDL_ConvertSurface(m_surface, windowSurface->format);
	SDL_BlitSurface(copy, &srcRect, windowSurface, &srcRect);
	SDL_DestroySurface(copy);
	SDL_UpdateWindowSurface(DDWindow);
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
	if (!m_palette) {
		return DDERR_GENERIC;
	}
	m_palette->AddRef();
	*lplpDDPalette = m_palette;
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat)
{
	memset(lpDDPixelFormat, 0, sizeof(*lpDDPixelFormat));
	lpDDPixelFormat->dwFlags = DDPF_RGB;
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc)
{
	if (!m_surface) {
		return DDERR_GENERIC;
	}
	const SDL_PixelFormatDetails* format = SDL_GetPixelFormatDetails(m_surface->format);
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
	if (!m_surface) {
		return DDERR_GENERIC;
	}

	if (SDL_LockSurface(m_surface) < 0) {
		return DDERR_GENERIC;
	}

	lpDDSurfaceDesc->lpSurface = m_surface->pixels;
	lpDDSurfaceDesc->lPitch = m_surface->pitch;

	const SDL_PixelFormatDetails* format = SDL_GetPixelFormatDetails(m_surface->format);
	lpDDSurfaceDesc->ddpfPixelFormat.dwFlags = DDPF_RGB;
	lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount = format->bits_per_pixel;
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
	if (!m_surface) {
		return DDERR_GENERIC;
	}
	return DD_OK;
}
