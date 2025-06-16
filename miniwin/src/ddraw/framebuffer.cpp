#include "ddpalette_impl.h"
#include "ddraw_impl.h"
#include "dummysurface_impl.h"
#include "framebuffer_impl.h"
#include "miniwin.h"

#include <assert.h>

FrameBufferImpl::FrameBufferImpl()
{
	int width, height;
	SDL_GetRenderOutputSize(DDRenderer, &width, &height);
	DDBackBuffer = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA8888);
	if (!DDBackBuffer) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create surface: %s", SDL_GetError());
	}
	m_uploadBuffer =
		SDL_CreateTexture(DDRenderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, width, height);
}

FrameBufferImpl::~FrameBufferImpl()
{
	SDL_DestroySurface(DDBackBuffer);
	if (m_palette) {
		m_palette->Release();
	}
}

// IUnknown interface
HRESULT FrameBufferImpl::QueryInterface(const GUID& riid, void** ppvObject)
{
	MINIWIN_NOT_IMPLEMENTED();
	return E_NOINTERFACE;
}

// IDirectDrawSurface interface
HRESULT FrameBufferImpl::AddAttachedSurface(LPDIRECTDRAWSURFACE lpDDSAttachedSurface)
{
	if (dynamic_cast<DummySurfaceImpl*>(lpDDSAttachedSurface)) {
		return DD_OK;
	}
	MINIWIN_NOT_IMPLEMENTED();
	return DDERR_GENERIC;
}

HRESULT FrameBufferImpl::Blt(
	LPRECT lpDestRect,
	LPDIRECTDRAWSURFACE lpDDSrcSurface,
	LPRECT lpSrcRect,
	DDBltFlags dwFlags,
	LPDDBLTFX lpDDBltFx
)
{
	if (dynamic_cast<FrameBufferImpl*>(lpDDSrcSurface) == this) {
		return Flip(nullptr, DDFLIP_WAIT);
	}
	if ((dwFlags & DDBLT_COLORFILL) == DDBLT_COLORFILL) {
		SDL_Rect rect = {0, 0, DDBackBuffer->w, DDBackBuffer->h};
		const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(DDBackBuffer->format);
		Uint8 r = (lpDDBltFx->dwFillColor >> 16) & 0xFF;
		Uint8 g = (lpDDBltFx->dwFillColor >> 8) & 0xFF;
		Uint8 b = lpDDBltFx->dwFillColor & 0xFF;
		DirectDrawPaletteImpl* ddPal = static_cast<DirectDrawPaletteImpl*>(m_palette);
		SDL_Palette* sdlPalette = ddPal ? ddPal->m_palette : nullptr;
		Uint32 color = SDL_MapRGB(details, sdlPalette, r, g, b);
		SDL_FillSurfaceRect(DDBackBuffer, &rect, color);
		return DD_OK;
	}
	auto other = static_cast<DirectDrawSurfaceImpl*>(lpDDSrcSurface);
	if (!other) {
		return DDERR_GENERIC;
	}
	SDL_Rect srcRect = lpSrcRect ? ConvertRect(lpSrcRect) : SDL_Rect{0, 0, other->m_surface->w, other->m_surface->h};
	SDL_Rect dstRect = lpDestRect ? ConvertRect(lpDestRect) : SDL_Rect{0, 0, DDBackBuffer->w, DDBackBuffer->h};

	SDL_Surface* blitSource = other->m_surface;

	if (other->m_surface->format != DDBackBuffer->format) {
		blitSource = SDL_ConvertSurface(other->m_surface, DDBackBuffer->format);
		if (!blitSource) {
			return DDERR_GENERIC;
		}
	}

	if (!SDL_BlitSurfaceScaled(blitSource, &srcRect, DDBackBuffer, &dstRect, SDL_SCALEMODE_NEAREST)) {
		return DDERR_GENERIC;
	}

	if (blitSource != other->m_surface) {
		SDL_DestroySurface(blitSource);
	}
	return DD_OK;
}

HRESULT FrameBufferImpl::BltFast(
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

HRESULT FrameBufferImpl::Flip(LPDIRECTDRAWSURFACE lpDDSurfaceTargetOverride, DDFlipFlags dwFlags)
{
	SDL_UpdateTexture(m_uploadBuffer, nullptr, DDBackBuffer->pixels, DDBackBuffer->pitch);
	SDL_RenderTexture(DDRenderer, m_uploadBuffer, nullptr, nullptr);
	SDL_RenderPresent(DDRenderer);
	return DD_OK;
}

HRESULT FrameBufferImpl::GetAttachedSurface(LPDDSCAPS lpDDSCaps, LPDIRECTDRAWSURFACE* lplpDDAttachedSurface)
{
	if ((lpDDSCaps->dwCaps & DDSCAPS_BACKBUFFER) != DDSCAPS_BACKBUFFER) {
		return DDERR_INVALIDPARAMS;
	}
	*lplpDDAttachedSurface = static_cast<IDirectDrawSurface*>(this);
	return DD_OK;
}

HRESULT FrameBufferImpl::GetDC(HDC* lphDC)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT FrameBufferImpl::GetPalette(LPDIRECTDRAWPALETTE* lplpDDPalette)
{
	if (!m_palette) {
		return DDERR_GENERIC;
	}
	m_palette->AddRef();
	*lplpDDPalette = m_palette;
	return DD_OK;
}

HRESULT FrameBufferImpl::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat)
{
	memset(lpDDPixelFormat, 0, sizeof(*lpDDPixelFormat));
	lpDDPixelFormat->dwFlags = DDPF_RGB;
	const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(DDBackBuffer->format);
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

HRESULT FrameBufferImpl::GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc)
{
	lpDDSurfaceDesc->dwFlags = DDSD_PIXELFORMAT;
	GetPixelFormat(&lpDDSurfaceDesc->ddpfPixelFormat);
	lpDDSurfaceDesc->dwFlags |= DDSD_WIDTH | DDSD_HEIGHT;
	lpDDSurfaceDesc->dwWidth = DDBackBuffer->w;
	lpDDSurfaceDesc->dwHeight = DDBackBuffer->h;
	return DD_OK;
}

HRESULT FrameBufferImpl::IsLost()
{
	return DD_OK;
}

HRESULT FrameBufferImpl::Lock(LPRECT lpDestRect, DDSURFACEDESC* lpDDSurfaceDesc, DDLockFlags dwFlags, HANDLE hEvent)
{
	if (!SDL_LockSurface(DDBackBuffer)) {
		return DDERR_GENERIC;
	}

	GetSurfaceDesc(lpDDSurfaceDesc);
	lpDDSurfaceDesc->lpSurface = DDBackBuffer->pixels;
	lpDDSurfaceDesc->lPitch = DDBackBuffer->pitch;

	return DD_OK;
}

HRESULT FrameBufferImpl::ReleaseDC(HDC hDC)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT FrameBufferImpl::Restore()
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT FrameBufferImpl::SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper)
{
	return DD_OK;
}

HRESULT FrameBufferImpl::SetColorKey(DDColorKeyFlags dwFlags, LPDDCOLORKEY lpDDColorKey)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT FrameBufferImpl::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette)
{
	if (DDBackBuffer->format != SDL_PIXELFORMAT_INDEX8) {
		MINIWIN_NOT_IMPLEMENTED();
	}

	if (m_palette) {
		m_palette->Release();
	}

	m_palette = lpDDPalette;
	SDL_SetSurfacePalette(DDBackBuffer, ((DirectDrawPaletteImpl*) m_palette)->m_palette);
	m_palette->AddRef();
	return DD_OK;
}

HRESULT FrameBufferImpl::Unlock(LPVOID lpSurfaceData)
{
	SDL_UnlockSurface(DDBackBuffer);
	return DD_OK;
}
