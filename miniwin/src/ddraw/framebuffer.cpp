#include "ddpalette_impl.h"
#include "ddraw_impl.h"
#include "dummysurface_impl.h"
#include "framebuffer_impl.h"
#include "miniwin.h"

#include <assert.h>

FrameBufferImpl::FrameBufferImpl(DWORD virtualWidth, DWORD virtualHeight)
	: m_virtualWidth(virtualWidth), m_virtualHeight(virtualHeight)
{
	m_transferBuffer = new DirectDrawSurfaceImpl(m_virtualWidth, m_virtualHeight, MORTAR_PIXELFORMAT_RGBA32);
}

FrameBufferImpl::~FrameBufferImpl()
{
	m_transferBuffer->Release();
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
	if (!DDRenderer) {
		return DDERR_GENERIC;
	}

	if (dynamic_cast<FrameBufferImpl*>(lpDDSrcSurface) == this) {
		return Flip(nullptr, DDFLIP_WAIT);
	}

	if ((dwFlags & DDBLT_COLORFILL) == DDBLT_COLORFILL) {
		uint8_t a = (lpDDBltFx->dwFillColor >> 24) & 0xFF;
		uint8_t r = (lpDDBltFx->dwFillColor >> 16) & 0xFF;
		uint8_t g = (lpDDBltFx->dwFillColor >> 8) & 0xFF;
		uint8_t b = lpDDBltFx->dwFillColor & 0xFF;

		float fa = a / 255.0f;
		float fr = r / 255.0f;
		float fg = g / 255.0f;
		float fb = b / 255.0f;

		if (lpDestRect) {
			MORTAR_Rect dstRect = ConvertRect(lpDestRect);
			DDRenderer->Draw2DImage(NO_TEXTURE_ID, MORTAR_Rect{}, dstRect, {fr, fg, fb, fa});
		}
		else {
			DDRenderer->Clear(fr, fg, fb);
		}
		return DD_OK;
	}

	auto surface = static_cast<DirectDrawSurfaceImpl*>(lpDDSrcSurface);
	if (!surface) {
		return DDERR_GENERIC;
	}
	MORTAR_Rect srcRect =
		lpSrcRect ? ConvertRect(lpSrcRect) : MORTAR_Rect{0, 0, surface->m_surface->w, surface->m_surface->h};
	MORTAR_Rect dstRect =
		lpDestRect ? ConvertRect(lpDestRect) : MORTAR_Rect{0, 0, (int) m_virtualWidth, (int) m_virtualHeight};
	float scaleX = (float) dstRect.w / (float) srcRect.w;
	float scaleY = (float) dstRect.h / (float) srcRect.h;
	uint32_t textureId = DDRenderer->GetTextureId(surface->ToTexture(), true, scaleX, scaleY);
	DDRenderer->Draw2DImage(textureId, srcRect, dstRect, {1.0f, 1.0f, 1.0f, 1.0f});

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
	auto surface = static_cast<DirectDrawSurfaceImpl*>(lpDDSrcSurface);
	int width = lpSrcRect ? (lpSrcRect->right - lpSrcRect->left) : surface->m_surface->w;
	int height = lpSrcRect ? (lpSrcRect->bottom - lpSrcRect->top) : surface->m_surface->h;
	RECT destRect = {(int) dwX, (int) dwY, (int) (dwX + width), (int) (dwY + height)};

	return Blt(&destRect, lpDDSrcSurface, lpSrcRect, DDBLT_NONE, nullptr);
}

HRESULT FrameBufferImpl::Flip(LPDIRECTDRAWSURFACE lpDDSurfaceTargetOverride, DDFlipFlags dwFlags)
{
	if (!DDRenderer) {
		return DDERR_GENERIC;
	}
	DDRenderer->Flip();
	return DD_OK;
}

HRESULT FrameBufferImpl::GetAttachedSurface(LPDDSCAPS lpDDSCaps, LPDIRECTDRAWSURFACE* lplpDDAttachedSurface)
{
	if ((lpDDSCaps->dwCaps & DDSCAPS_BACKBUFFER) != DDSCAPS_BACKBUFFER) {
		return DDERR_INVALIDPARAMS;
	}
	AddRef();
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
	lpDDPixelFormat->dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
	const MORTAR_PixelFormatDetails* details = MORTAR_GetPixelFormatDetails(m_transferBuffer->m_surface->format);
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
	lpDDSurfaceDesc->dwWidth = m_transferBuffer->m_surface->w;
	lpDDSurfaceDesc->dwHeight = m_transferBuffer->m_surface->h;
	return DD_OK;
}

HRESULT FrameBufferImpl::Lock(LPRECT lpDestRect, DDSURFACEDESC* lpDDSurfaceDesc, DDLockFlags dwFlags, HANDLE hEvent)
{
	if (!DDRenderer) {
		return DDERR_GENERIC;
	}

	m_readOnlyLock = (dwFlags & DDLOCK_READONLY) == DDLOCK_READONLY;

	if ((dwFlags & DDLOCK_WRITEONLY) == DDLOCK_WRITEONLY) {
		const MORTAR_PixelFormatDetails* details = MORTAR_GetPixelFormatDetails(m_transferBuffer->m_surface->format);
		MORTAR_Palette* palette = m_palette ? static_cast<DirectDrawPaletteImpl*>(m_palette)->m_palette : nullptr;
		uint32_t color = MORTAR_MapRGBA(details, palette, 0, 0, 0, 0);
		MORTAR_FillSurfaceRect(m_transferBuffer->m_surface, nullptr, color);
	}
	else {
		DDRenderer->Download(m_transferBuffer->m_surface);
	}

	m_transferBuffer->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);

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

HRESULT FrameBufferImpl::SetColorKey(DDColorKeyFlags dwFlags, LPDDCOLORKEY lpDDColorKey)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT FrameBufferImpl::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette)
{
	if (m_transferBuffer->m_surface->format != MORTAR_PIXELFORMAT_INDEX8) {
		MINIWIN_NOT_IMPLEMENTED();
	}

	lpDDPalette->AddRef();

	if (m_palette) {
		m_palette->Release();
	}

	m_palette = lpDDPalette;
	MORTAR_SetSurfacePalette(m_transferBuffer->m_surface, ((DirectDrawPaletteImpl*) m_palette)->m_palette);
	return DD_OK;
}

HRESULT FrameBufferImpl::Unlock(LPVOID lpSurfaceData)
{
	m_transferBuffer->Unlock(lpSurfaceData);
	if (!m_readOnlyLock) {
		BltFast(0, 0, m_transferBuffer, nullptr, DDBLTFAST_WAIT);
	}

	return DD_OK;
}
