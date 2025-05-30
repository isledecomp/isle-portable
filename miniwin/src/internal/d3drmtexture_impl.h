#pragma once

#include "d3drmobject_impl.h"

struct Direct3DRMTextureImpl : public Direct3DRMObjectBaseImpl<IDirect3DRMTexture2> {
	Direct3DRMTextureImpl(D3DRMIMAGE* image);
	Direct3DRMTextureImpl(IDirectDrawSurface* surface);
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override;
	HRESULT Changed(BOOL pixels, BOOL palette) override;

private:
	IDirectDrawSurface* m_surface = nullptr;
};
