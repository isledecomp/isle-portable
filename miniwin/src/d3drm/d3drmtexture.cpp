#include "d3drmtexture_impl.h"
#include "miniwin.h"

Direct3DRMTextureImpl::Direct3DRMTextureImpl(D3DRMIMAGE* image)
{
	MINIWIN_NOT_IMPLEMENTED();
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
