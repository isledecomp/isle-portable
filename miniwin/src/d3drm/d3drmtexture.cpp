#include "d3drmtexture_impl.h"
#include "miniwin.h"

Direct3DRMTextureImpl::Direct3DRMTextureImpl(D3DRMIMAGE* image)
{
	MINIWIN_NOT_IMPLEMENTED();
}

Direct3DRMTextureImpl::Direct3DRMTextureImpl(IDirectDrawSurface* surface) : m_surface(surface)
{
	MINIWIN_NOT_IMPLEMENTED();
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
