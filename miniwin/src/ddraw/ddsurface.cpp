#include "ddpalette_impl.h"
#include "ddraw_impl.h"
#include "ddsurface_impl.h"
#include "miniwin.h"

#include <assert.h>

DirectDrawSurfaceImpl::DirectDrawSurfaceImpl(int width, int height, SDL_PixelFormat format)
{
	m_texture = SDL_CreateTexture(DDRenderer, format, SDL_TEXTUREACCESS_STREAMING, width, height);
	if (!m_texture) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create texture: %s", SDL_GetError());
	}
}

DirectDrawSurfaceImpl::~DirectDrawSurfaceImpl()
{
	SDL_DestroyTexture(m_texture);
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
HRESULT DirectDrawSurfaceImpl::AddAttachedSurface(IDirectDrawSurface* lpDDSAttachedSurface)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

static SDL_Rect ConvertRect(const RECT* r)
{
	return {r->left, r->top, (r->right - r->left), (r->bottom - r->top)};
}

HRESULT DirectDrawSurfaceImpl::Blt(
	LPRECT lpDestRect,
	IDirectDrawSurface* lpDDSrcSurface,
	LPRECT lpSrcRect,
	DDBltFlags dwFlags,
	LPDDBLTFX lpDDBltFx
)
{
	auto* other = dynamic_cast<DirectDrawSurfaceImpl*>(lpDDSrcSurface);
	if (!other) {
		MINIWIN_NOT_IMPLEMENTED();
		return DDERR_GENERIC;
	}

	SDL_Rect srcRect, dstRect;
	if (lpSrcRect) {
		srcRect = ConvertRect(lpSrcRect);
	}
	if (lpDestRect) {
		dstRect = ConvertRect(lpDestRect);
	}

	void* otherPixels;
	int otherPitch;
	if (!SDL_LockTexture(other->m_texture, lpSrcRect ? &srcRect : nullptr, &otherPixels, &otherPitch)) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to lock other texture: %s", SDL_GetError());
		return DDERR_GENERIC;
	}
	void* pixels;
	int pitch;
	if (!SDL_LockTexture(m_texture, lpDestRect ? &dstRect : nullptr, &pixels, &pitch)) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to lock destination texture: %s", SDL_GetError());
		return DDERR_GENERIC;
	}
	memcpy(pixels, otherPixels, (lpDestRect ? dstRect.h : m_texture->h) * pitch);
	SDL_UnlockTexture(other->m_texture);
	SDL_UnlockTexture(m_texture);

	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::BltFast(
	DWORD dwX,
	DWORD dwY,
	IDirectDrawSurface* lpDDSrcSurface,
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

HRESULT DirectDrawSurfaceImpl::Flip(IDirectDrawSurface* lpDDSurfaceTargetOverride, DDFlipFlags dwFlags)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::GetAttachedSurface(LPDDSCAPS lpDDSCaps, IDirectDrawSurface** lplpDDAttachedSurface)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::GetDC(HDC* lphDC)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::GetPalette(LPDIRECTDRAWPALETTE* lplpDDPalette)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat)
{
	lpDDPixelFormat->dwFlags = DDPF_RGB;
	const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(HWBackBufferFormat);
	if (details->bits_per_pixel == 8) {
		lpDDPixelFormat->dwFlags |= DDPF_PALETTEINDEXED8;
	}
	lpDDPixelFormat->dwRGBBitCount = details->bits_per_pixel;
	lpDDPixelFormat->dwRBitMask = details->Rmask;
	lpDDPixelFormat->dwGBitMask = details->Gmask;
	lpDDPixelFormat->dwBBitMask = details->Bmask;
	lpDDPixelFormat->dwRGBAlphaBitMask = details->Amask;
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::GetSurfaceDesc(DDSURFACEDESC* lpDDSurfaceDesc)
{
	lpDDSurfaceDesc->dwFlags = DDSD_PIXELFORMAT;
	GetPixelFormat(&lpDDSurfaceDesc->ddpfPixelFormat);
	lpDDSurfaceDesc->dwFlags |= DDSD_WIDTH | DDSD_HEIGHT;
	lpDDSurfaceDesc->dwWidth = m_texture->w;
	lpDDSurfaceDesc->dwHeight = m_texture->h;
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::IsLost()
{
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::Lock(
	LPRECT lpDestRect,
	DDSURFACEDESC* lpDDSurfaceDesc,
	DDLockFlags dwFlags,
	HANDLE hEvent
)
{
	if (!SDL_LockTexture(m_texture, nullptr, &lpDDSurfaceDesc->lpSurface, &lpDDSurfaceDesc->lPitch)) {
		return DDERR_GENERIC;
	}

	GetSurfaceDesc(lpDDSurfaceDesc);
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::ReleaseDC(HDC hDC)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::Restore()
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::SetClipper(IDirectDrawClipper* lpDDClipper)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::SetColorKey(DDColorKeyFlags dwFlags, LPDDCOLORKEY lpDDColorKey)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::Unlock(LPVOID lpSurfaceData)
{
	SDL_UnlockTexture(m_texture);
	return DD_OK;
}
