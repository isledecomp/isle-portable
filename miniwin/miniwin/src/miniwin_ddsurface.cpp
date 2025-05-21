#include "miniwin_ddpalette_p.h"
#include "miniwin_ddraw_p.h"
#include "miniwin_ddsurface_p.h"
#include "miniwin_p.h"

#include <assert.h>

DirectDrawSurfaceImpl::DirectDrawSurfaceImpl()
{
}

DirectDrawSurfaceImpl::DirectDrawSurfaceImpl(int width, int height, SDL_PixelFormat format)
{
	m_surface = SDL_CreateSurface(width, height, format);
	if (!m_surface) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create surface: %s", SDL_GetError());
	}
}

DirectDrawSurfaceImpl::~DirectDrawSurfaceImpl()
{
	if (m_surface) {
		SDL_DestroySurface(m_surface);
	}
	if (m_palette) {
		m_palette->Release();
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
	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DirectDrawSurfaceImpl does not implement guid");
	return E_NOINTERFACE;
}

// IDirectDrawSurface interface
HRESULT DirectDrawSurfaceImpl::AddAttachedSurface(LPDIRECTDRAWSURFACE lpDDSAttachedSurface)
{
	return DD_OK;
}

void DirectDrawSurfaceImpl::SetAutoFlip(bool enabled)
{
	m_autoFlip = enabled;
}

static SDL_Rect ConvertRect(const RECT* r)
{
	return {r->left, r->top, r->right - r->left, r->bottom - r->top};
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
	if (m_autoFlip) {
		DDBackBuffer = srcSurface->m_surface;
		return Flip(nullptr, DDFLIP_WAIT);
	}

	SDL_Rect srcRect;
	if (lpSrcRect) {
		srcRect = ConvertRect(lpSrcRect);
	}
	else {
		srcRect = {0, 0, srcSurface->m_surface->w, srcSurface->m_surface->h};
	}

	SDL_Rect dstRect;
	if (lpDestRect) {
		dstRect = ConvertRect(lpDestRect);
	}
	else {
		dstRect = {0, 0, m_surface->w, m_surface->h};
	}

	SDL_Surface* blitSource = srcSurface->m_surface;

	if (srcSurface->m_surface->format != m_surface->format) {
		blitSource = SDL_ConvertSurface(srcSurface->m_surface, m_surface->format);
		if (!blitSource) {
			return DDERR_GENERIC;
		}
	}

	if (!SDL_BlitSurfaceScaled(blitSource, &srcRect, m_surface, &dstRect, SDL_SCALEMODE_NEAREST)) {
		return DDERR_GENERIC;
	}

	if (blitSource != srcSurface->m_surface) {
		SDL_DestroySurface(blitSource);
	}
	if (m_autoFlip) {
		return Flip(nullptr, DDFLIP_WAIT);
	}
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
	RECT destRect = {
		(int) dwX,
		(int) dwY,
		(int) (lpSrcRect->right - lpSrcRect->left + dwX),
		(int) (lpSrcRect->bottom - lpSrcRect->top + dwY)
	};
	return Blt(&destRect, lpDDSrcSurface, lpSrcRect, DDBLT_NONE, nullptr);
}

HRESULT DirectDrawSurfaceImpl::Flip(LPDIRECTDRAWSURFACE lpDDSurfaceTargetOverride, DDFlipFlags dwFlags)
{
	if (!DDBackBuffer) {
		return DDERR_GENERIC;
	}
	SDL_Surface* windowSurface = SDL_GetWindowSurface(DDWindow);
	if (!windowSurface) {
		return DDERR_GENERIC;
	}
	SDL_Rect srcRect{0, 0, DDBackBuffer->w, DDBackBuffer->h};
	SDL_Surface* copy = SDL_ConvertSurface(DDBackBuffer, windowSurface->format);
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
	DDBackBuffer = m_surface;
	*lplpDDAttachedSurface = static_cast<IDirectDrawSurface*>(this);
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::GetDC(HDC* lphDC)
{
	MINIWIN_NOT_IMPLEMENTED();
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
	if (!m_surface) {
		return DDERR_GENERIC;
	}
	memset(lpDDPixelFormat, 0, sizeof(*lpDDPixelFormat));
	lpDDPixelFormat->dwFlags = DDPF_RGB;
	const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(m_surface->format);
	if (details->bits_per_pixel == 8) {
		lpDDPixelFormat->dwFlags |= DDPF_PALETTEINDEXED8;
	}
	lpDDPixelFormat->dwRGBBitCount = details->bits_per_pixel;
	lpDDPixelFormat->dwRBitMask = details->Rmask;
	lpDDPixelFormat->dwGBitMask = details->Gmask;
	lpDDPixelFormat->dwBBitMask = details->Bmask;
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc)
{
	if (!m_surface) {
		return DDERR_GENERIC;
	}
	lpDDSurfaceDesc->dwFlags = DDSD_PIXELFORMAT;
	GetPixelFormat(&lpDDSurfaceDesc->ddpfPixelFormat);
	if (m_surface) {
		lpDDSurfaceDesc->dwFlags |= DDSD_WIDTH | DDSD_HEIGHT;
		lpDDSurfaceDesc->dwWidth = m_surface->w;
		lpDDSurfaceDesc->dwHeight = m_surface->h;
	}
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

	GetSurfaceDesc(lpDDSurfaceDesc);
	lpDDSurfaceDesc->lpSurface = m_surface->pixels;
	lpDDSurfaceDesc->lPitch = m_surface->pitch;

	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::ReleaseDC(HDC hDC)
{
	MINIWIN_NOT_IMPLEMENTED();
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
	if (lpDDColorKey->dwColorSpaceLowValue != lpDDColorKey->dwColorSpaceHighValue) {
		MINIWIN_NOT_IMPLEMENTED();
	}

	if (SDL_SetSurfaceColorKey(m_surface, true, lpDDColorKey->dwColorSpaceLowValue) != 0) {
		return DDERR_GENERIC;
	}

	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette)
{
	if (m_surface->format != SDL_PIXELFORMAT_INDEX8) {
		MINIWIN_NOT_IMPLEMENTED();
	}

	if (m_palette) {
		m_palette->Release();
	}

	m_palette = lpDDPalette;
	SDL_SetSurfacePalette(m_surface, ((DirectDrawPaletteImpl*) m_palette)->m_palette);
	m_palette->AddRef();
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::Unlock(LPVOID lpSurfaceData)
{
	if (!m_surface) {
		return DDERR_GENERIC;
	}
	SDL_UnlockSurface(m_surface);
	return DD_OK;
}
