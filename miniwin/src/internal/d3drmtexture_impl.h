#pragma once

#include "d3drmobject_impl.h"

struct Direct3DRMTextureImpl : public Direct3DRMObjectBaseImpl<IDirect3DRMTexture2> {
	Direct3DRMTextureImpl(D3DRMIMAGE* image);
	Direct3DRMTextureImpl(IDirectDrawSurface* surface, bool holdsRef);
	~Direct3DRMTextureImpl() override;
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override;
	HRESULT Changed(BOOL pixels, BOOL palette) override;

	IDirectDrawSurface* m_surface = nullptr;
	Uint8 m_version = 0;
	bool m_holdsRef;
};
