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
	m_backBuffer = SDL_CreateTexture(DDRenderer, HWBackBufferFormat, SDL_TEXTUREACCESS_TARGET, width, height);
	m_uploadBuffer = SDL_CreateTexture(DDRenderer, HWBackBufferFormat, SDL_TEXTUREACCESS_STREAMING, width, height);
	SDL_SetRenderTarget(DDRenderer, m_backBuffer);
	if (!m_backBuffer) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create surface: %s", SDL_GetError());
	}
}

FrameBufferImpl::~FrameBufferImpl()
{
	SDL_DestroyTexture(m_backBuffer);
}

// IUnknown interface
HRESULT FrameBufferImpl::QueryInterface(const GUID& riid, void** ppvObject)
{
	MINIWIN_NOT_IMPLEMENTED();
	return E_NOINTERFACE;
}

// IDirectDrawSurface interface
HRESULT FrameBufferImpl::AddAttachedSurface(IDirectDrawSurface* lpDDSAttachedSurface)
{
	if (dynamic_cast<DummySurfaceImpl*>(lpDDSAttachedSurface)) {
		return DD_OK;
	}
	MINIWIN_NOT_IMPLEMENTED();
	return DDERR_GENERIC;
}

static SDL_FRect ConvertRect(const RECT* r)
{
	return {(float) r->left, (float) r->top, (float) (r->right - r->left), (float) (r->bottom - r->top)};
}

HRESULT FrameBufferImpl::Blt(
	LPRECT lpDestRect,
	IDirectDrawSurface* lpDDSrcSurface,
	LPRECT lpSrcRect,
	DDBltFlags dwFlags,
	LPDDBLTFX lpDDBltFx
)
{
	if (dynamic_cast<FrameBufferImpl*>(lpDDSrcSurface) == this) {
		return Flip(nullptr, DDFLIP_WAIT);
	}
	if ((dwFlags & DDBLT_COLORFILL) == DDBLT_COLORFILL) {
		if (lpDestRect != nullptr) {
			MINIWIN_NOT_IMPLEMENTED();
		}
		Uint8 r = (lpDDBltFx->dwFillColor >> 16) & 0xFF;
		Uint8 g = (lpDDBltFx->dwFillColor >> 8) & 0xFF;
		Uint8 b = lpDDBltFx->dwFillColor & 0xFF;
		SDL_SetRenderDrawColor(DDRenderer, r, g, b, 0xFF);
		SDL_RenderClear(DDRenderer);
	}
	else {
		auto srcSurface = dynamic_cast<DirectDrawSurfaceImpl*>(lpDDSrcSurface);
		if (!srcSurface) {
			return DDERR_GENERIC;
		}
		SDL_FRect srcRect, dstRect;
		if (lpSrcRect) {
			srcRect = ConvertRect(lpSrcRect);
		}
		if (lpDestRect) {
			dstRect = ConvertRect(lpDestRect);
		}
		if (!SDL_RenderTexture(
				DDRenderer,
				srcSurface->m_texture,
				lpSrcRect ? &srcRect : nullptr,
				lpDestRect ? &dstRect : nullptr
			)) {
			return DDERR_GENERIC;
		}
	}
	return DD_OK;
}

HRESULT FrameBufferImpl::BltFast(
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

HRESULT FrameBufferImpl::Flip(IDirectDrawSurface* lpDDSurfaceTargetOverride, DDFlipFlags dwFlags)
{
	SDL_SetRenderTarget(DDRenderer, nullptr);
	SDL_RenderTexture(DDRenderer, m_backBuffer, nullptr, nullptr);
	SDL_RenderPresent(DDRenderer);
	SDL_SetRenderTarget(DDRenderer, m_backBuffer);
	return DD_OK;
}

HRESULT FrameBufferImpl::GetAttachedSurface(LPDDSCAPS lpDDSCaps, IDirectDrawSurface** lplpDDAttachedSurface)
{
	if ((lpDDSCaps->dwCaps & DDSCAPS_BACKBUFFER) != DDSCAPS_BACKBUFFER) {
		return DDERR_INVALIDPARAMS;
	}
	this->AddRef();
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
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT FrameBufferImpl::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat)
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

HRESULT FrameBufferImpl::GetSurfaceDesc(DDSURFACEDESC* lpDDSurfaceDesc)
{
	lpDDSurfaceDesc->dwFlags = DDSD_PIXELFORMAT;
	GetPixelFormat(&lpDDSurfaceDesc->ddpfPixelFormat);
	lpDDSurfaceDesc->dwFlags |= DDSD_WIDTH | DDSD_HEIGHT;
	lpDDSurfaceDesc->dwWidth = m_backBuffer->w;
	lpDDSurfaceDesc->dwHeight = m_backBuffer->h;
	return DD_OK;
}

HRESULT FrameBufferImpl::IsLost()
{
	return DD_OK;
}

HRESULT FrameBufferImpl::Lock(LPRECT lpDestRect, DDSURFACEDESC* lpDDSurfaceDesc, DDLockFlags dwFlags, HANDLE hEvent)
{
	m_readBackBuffer = SDL_RenderReadPixels(DDRenderer, nullptr);
	if (!SDL_LockSurface(m_readBackBuffer)) {
		return DDERR_GENERIC;
	}

	GetSurfaceDesc(lpDDSurfaceDesc);
	lpDDSurfaceDesc->lpSurface = m_readBackBuffer->pixels;
	lpDDSurfaceDesc->lPitch = m_readBackBuffer->pitch;

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

HRESULT FrameBufferImpl::SetClipper(IDirectDrawClipper* lpDDClipper)
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
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT FrameBufferImpl::Unlock(LPVOID lpSurfaceData)
{
	SDL_UpdateTexture(m_uploadBuffer, nullptr, m_readBackBuffer->pixels, m_readBackBuffer->pitch);
	SDL_DestroySurface(m_readBackBuffer);
	SDL_RenderTexture(DDRenderer, m_uploadBuffer, nullptr, nullptr);
	return DD_OK;
}

void FrameBufferImpl::Upload(void* pixels, int pitch)
{
	SDL_UpdateTexture(m_uploadBuffer, nullptr, pixels, pitch);
	SDL_RenderTexture(DDRenderer, m_uploadBuffer, nullptr, nullptr);
}
