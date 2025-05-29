#include "d3drmtexture_impl.h"
#include "miniwin.h"

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
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}
