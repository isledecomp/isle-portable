#include "d3drmtexture_impl.h"
#include "ddpalette_impl.h"
#include "ddraw_impl.h"
#include "ddsurface_impl.h"
#include "miniwin.h"

#include <cassert>
#include <mortar/mortar.h>

DirectDrawSurfaceImpl::DirectDrawSurfaceImpl(int width, int height, MORTAR_PixelFormat format)
{
	m_surface = MORTAR_CreateSurface(width, height, format);
	if (!m_surface) {
		MORTAR_LogError(MORTAR_LOG_CATEGORY_APPLICATION, "Failed to create surface: %s", MORTAR_GetError());
	}
}

DirectDrawSurfaceImpl::~DirectDrawSurfaceImpl()
{
	MORTAR_DestroySurface(m_surface);
	if (m_palette) {
		m_palette->Release();
	}
	if (m_texture) {
		m_texture->Release();
	}
}

// IUnknown interface
HRESULT DirectDrawSurfaceImpl::QueryInterface(const GUID& riid, void** ppvObject)
{
	if (MORTAR_memcmp(&riid, &IID_IDirectDrawSurface3, sizeof(GUID)) == 0) {
		this->IUnknown::AddRef();
		*ppvObject = static_cast<IDirectDrawSurface3*>(this);
		return S_OK;
	}
	MORTAR_LogError(MORTAR_LOG_CATEGORY_APPLICATION, "DirectDrawSurfaceImpl does not implement guid");
	return E_NOINTERFACE;
}

// IDirectDrawSurface interface
HRESULT DirectDrawSurfaceImpl::AddAttachedSurface(LPDIRECTDRAWSURFACE lpDDSAttachedSurface)
{
	MINIWIN_NOT_IMPLEMENTED();
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
	if ((dwFlags & DDBLT_COLORFILL) == DDBLT_COLORFILL) {
		uint8_t a = (lpDDBltFx->dwFillColor >> 24) & 0xFF;
		uint8_t r = (lpDDBltFx->dwFillColor >> 16) & 0xFF;
		uint8_t g = (lpDDBltFx->dwFillColor >> 8) & 0xFF;
		uint8_t b = lpDDBltFx->dwFillColor & 0xFF;

		const MORTAR_PixelFormatDetails* details = MORTAR_GetPixelFormatDetails(m_surface->format);
		uint32_t color = MORTAR_MapRGBA(details, nullptr, r, g, b, a);
		if (lpDestRect) {
			MORTAR_Rect dstRect = ConvertRect(lpDestRect);
			MORTAR_FillSurfaceRect(m_surface, &dstRect, color);
		}
		else {
			MORTAR_FillSurfaceRect(m_surface, nullptr, color);
		}
		return DD_OK;
	}

	auto other = static_cast<DirectDrawSurfaceImpl*>(lpDDSrcSurface);

	MORTAR_Rect srcRect =
		lpSrcRect ? ConvertRect(lpSrcRect) : MORTAR_Rect{0, 0, other->m_surface->w, other->m_surface->h};
	MORTAR_Rect dstRect = lpDestRect ? ConvertRect(lpDestRect) : MORTAR_Rect{0, 0, m_surface->w, m_surface->h};

	MORTAR_Surface* blitSource = other->m_surface;

	if (other->m_surface->format != m_surface->format) {
		blitSource = MORTAR_ConvertSurface(other->m_surface, m_surface->format);
		if (!blitSource) {
			return DDERR_GENERIC;
		}
	}

	if (!MORTAR_BlitSurfaceScaled(blitSource, &srcRect, m_surface, &dstRect, MORTAR_SCALEMODE_NEAREST)) {
		return DDERR_GENERIC;
	}

	if (blitSource != other->m_surface) {
		MORTAR_DestroySurface(blitSource);
	}
	if (m_texture) {
		m_texture->Changed(TRUE, FALSE);
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
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::GetAttachedSurface(LPDDSCAPS lpDDSCaps, LPDIRECTDRAWSURFACE* lplpDDAttachedSurface)
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
	lpDDPixelFormat->dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
	const MORTAR_PixelFormatDetails* details = MORTAR_GetPixelFormatDetails(m_surface->format);
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

HRESULT DirectDrawSurfaceImpl::GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc)
{
	lpDDSurfaceDesc->dwFlags = DDSD_PIXELFORMAT;
	GetPixelFormat(&lpDDSurfaceDesc->ddpfPixelFormat);
	lpDDSurfaceDesc->dwFlags |= DDSD_WIDTH | DDSD_HEIGHT;
	lpDDSurfaceDesc->dwWidth = m_surface->w;
	lpDDSurfaceDesc->dwHeight = m_surface->h;
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
	if (!MORTAR_LockSurface(m_surface)) {
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
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::SetColorKey(DDColorKeyFlags dwFlags, LPDDCOLORKEY lpDDColorKey)
{
	if (lpDDColorKey->dwColorSpaceLowValue != lpDDColorKey->dwColorSpaceHighValue) {
		MINIWIN_NOT_IMPLEMENTED();
	}

	if (MORTAR_SetSurfaceColorKey(m_surface, true, lpDDColorKey->dwColorSpaceLowValue) != 0) {
		return DDERR_GENERIC;
	}

	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette)
{
	if (m_surface->format != MORTAR_PIXELFORMAT_INDEX8) {
		MINIWIN_NOT_IMPLEMENTED();
	}

	if (m_texture) {
		m_texture->Changed(FALSE, TRUE);
	}

	lpDDPalette->AddRef();

	if (m_palette) {
		m_palette->Release();
	}

	m_palette = lpDDPalette;
	MORTAR_SetSurfacePalette(m_surface, ((DirectDrawPaletteImpl*) m_palette)->m_palette);
	return DD_OK;
}

HRESULT DirectDrawSurfaceImpl::Unlock(LPVOID lpSurfaceData)
{
	MORTAR_UnlockSurface(m_surface);
	if (m_texture) {
		m_texture->Changed(TRUE, FALSE);
	}
	return DD_OK;
}

IDirect3DRMTexture2* DirectDrawSurfaceImpl::ToTexture()
{
	if (!m_texture) {
		m_texture = new Direct3DRMTextureImpl(this, false);
	}
	return m_texture;
}
