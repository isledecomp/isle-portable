#pragma once

#include "d3drmobject_impl.h"

struct Direct3DRMTextureImpl : public Direct3DRMObjectBaseImpl<IDirect3DRMTexture2> {
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override;
	HRESULT Changed(BOOL pixels, BOOL palette) override;
};
