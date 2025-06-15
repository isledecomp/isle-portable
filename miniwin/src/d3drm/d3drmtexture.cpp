#include "d3drmtexture_impl.h"
#include "ddpalette_impl.h"
#include "miniwin.h"

Direct3DRMTextureImpl::Direct3DRMTextureImpl(int width, int height, SDL_PixelFormat format)
{
	m_surface = SDL_CreateSurface(width, height, format);
	if (!m_surface) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create surface: %s", SDL_GetError());
	}
}

Direct3DRMTextureImpl::~Direct3DRMTextureImpl()
{
	SDL_DestroySurface(m_surface);
}

HRESULT Direct3DRMTextureImpl::QueryInterface(const GUID& riid, void** ppvObject)
{
	if (SDL_memcmp(&riid, &IID_IDirect3DRMTexture2, sizeof(GUID)) == 0) {
		this->IUnknown::AddRef();
		*ppvObject = static_cast<IDirect3DRMTexture2*>(this);
		return DD_OK;
	}
	MINIWIN_NOT_IMPLEMENTED();
	return E_NOINTERFACE;
}

HRESULT Direct3DRMTextureImpl::Changed(BOOL pixels, BOOL palette)
{
	m_version++;
	return DD_OK;
}

// IDirectDrawSurface interface
HRESULT Direct3DRMTextureImpl::AddAttachedSurface(IDirectDrawSurface* lpDDSAttachedSurface)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DDERR_GENERIC;
}

HRESULT Direct3DRMTextureImpl::Blt(
	LPRECT lpDestRect,
	IDirectDrawSurface* lpDDSrcSurface,
	LPRECT lpSrcRect,
	DDBltFlags dwFlags,
	LPDDBLTFX lpDDBltFx
)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DDERR_GENERIC;
}

HRESULT Direct3DRMTextureImpl::BltFast(
	DWORD dwX,
	DWORD dwY,
	IDirectDrawSurface* lpDDSrcSurface,
	LPRECT lpSrcRect,
	DDBltFastFlags dwTrans
)
{
	if (dwX != 0 || dwY != 0) {
		MINIWIN_NOT_IMPLEMENTED();
		return DDERR_GENERIC;
	}
	auto* other = dynamic_cast<Direct3DRMTextureImpl*>(lpDDSrcSurface);
	if (!other) {
		MINIWIN_NOT_IMPLEMENTED();
		return DDERR_GENERIC;
	}
	if (other->m_surface->format != m_surface->format) {
		MINIWIN_NOT_IMPLEMENTED();
		return DDERR_GENERIC;
	}
	SDL_BlitSurface(other->m_surface, nullptr, m_surface, nullptr);
	return DD_OK;
}

HRESULT Direct3DRMTextureImpl::Flip(IDirectDrawSurface* lpDDSurfaceTarget, DDFlipFlags dwFlags)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DDERR_GENERIC;
}

HRESULT Direct3DRMTextureImpl::GetAttachedSurface(LPDDSCAPS lpDDSCaps, IDirectDrawSurface** lplpDDAttachedSurface)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DDERR_GENERIC;
}

HRESULT Direct3DRMTextureImpl::GetDC(HDC* lphDC)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DDERR_GENERIC;
}

HRESULT Direct3DRMTextureImpl::GetPalette(LPDIRECTDRAWPALETTE* lplpDDPalette)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DDERR_GENERIC;
}

HRESULT Direct3DRMTextureImpl::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat)
{
	lpDDPixelFormat->dwFlags = DDPF_RGB;
	const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(m_surface->format);
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

HRESULT Direct3DRMTextureImpl::GetSurfaceDesc(DDSURFACEDESC* lpDDSurfaceDesc)
{
	lpDDSurfaceDesc->dwFlags = DDSD_PIXELFORMAT;
	GetPixelFormat(&lpDDSurfaceDesc->ddpfPixelFormat);
	lpDDSurfaceDesc->dwFlags |= DDSD_WIDTH | DDSD_HEIGHT;
	lpDDSurfaceDesc->dwWidth = m_surface->w;
	lpDDSurfaceDesc->dwHeight = m_surface->h;
	return DD_OK;
}

HRESULT Direct3DRMTextureImpl::IsLost()
{
	MINIWIN_NOT_IMPLEMENTED();
	return DDERR_GENERIC;
}

HRESULT Direct3DRMTextureImpl::Lock(
	LPRECT lpDestRect,
	DDSURFACEDESC* lpDDSurfaceDesc,
	DDLockFlags dwFlags,
	HANDLE hEvent
)
{
	if (!SDL_LockSurface(m_surface)) {
		return DDERR_GENERIC;
	}

	GetSurfaceDesc(lpDDSurfaceDesc);
	lpDDSurfaceDesc->lpSurface = m_surface->pixels;
	lpDDSurfaceDesc->lPitch = m_surface->pitch;

	return DD_OK;
}

HRESULT Direct3DRMTextureImpl::ReleaseDC(HDC hDC)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DDERR_GENERIC;
}

HRESULT Direct3DRMTextureImpl::Restore()
{
	MINIWIN_NOT_IMPLEMENTED();
	return DDERR_GENERIC;
}

HRESULT Direct3DRMTextureImpl::SetClipper(IDirectDrawClipper* lpDDClipper)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DDERR_GENERIC;
}

HRESULT Direct3DRMTextureImpl::SetColorKey(DDColorKeyFlags dwFlags, LPDDCOLORKEY lpDDColorKey)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DDERR_GENERIC;
}

HRESULT Direct3DRMTextureImpl::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette)
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

HRESULT Direct3DRMTextureImpl::Unlock(LPVOID lpSurfaceData)
{
	SDL_UnlockSurface(m_surface);
	return DD_OK;
}
