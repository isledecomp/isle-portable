#include "d3drmtexture_impl.h"
#include "ddsurface_impl.h"
#include "miniwin.h"

Direct3DRMTextureImpl::Direct3DRMTextureImpl(D3DRMIMAGE* image) : m_holdsRef(true)
{
	if (!image || !image->buffer1 || image->width == 0 || image->height == 0) {
		m_surface = nullptr;
		return;
	}

	auto* ddsurface = new DirectDrawSurfaceImpl(image->width, image->height, SDL_PIXELFORMAT_RGBA32);
	if (!ddsurface->m_surface) {
		ddsurface->Release();
		m_surface = nullptr;
		return;
	}

	uint8_t* dst = static_cast<uint8_t*>(ddsurface->m_surface->pixels);
	int dstPitch = ddsurface->m_surface->pitch;

	if (!image->rgb && image->palette && image->palette_size > 0) {
		uint8_t* indices = static_cast<uint8_t*>(image->buffer1);
		for (int y = 0; y < image->height; y++) {
			for (int x = 0; x < image->width; x++) {
				uint8_t idx = indices[y * image->bytes_per_line + x];
				uint8_t* pixel = dst + y * dstPitch + x * 4;
				if (idx < image->palette_size) {
					const D3DRMPALETTEENTRY& e = image->palette[idx];
					pixel[0] = e.red;
					pixel[1] = e.green;
					pixel[2] = e.blue;
					// Palette index 0 is transparent by convention (color key)
					pixel[3] = (idx == 0) ? 0 : 255;
				}
				else {
					pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0;
				}
			}
		}
	}
	else {
		SDL_memset(dst, 0, dstPitch * image->height);
	}

	m_surface = static_cast<IDirectDrawSurface*>(ddsurface);
}

Direct3DRMTextureImpl::Direct3DRMTextureImpl(IDirectDrawSurface* surface, bool holdsRef)
	: m_surface(surface), m_holdsRef(holdsRef)
{
	if (holdsRef && m_surface) {
		m_surface->AddRef();
	}
}

Direct3DRMTextureImpl::~Direct3DRMTextureImpl()
{
	if (m_holdsRef && m_surface) {
		m_surface->Release();
	}
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
	if (!m_surface) {
		return DDERR_GENERIC;
	}
	m_version++;
	return DD_OK;
}
