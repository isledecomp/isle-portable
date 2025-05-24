#pragma once

#include "miniwin_d3drmobject_p.h"

struct Direct3DRMTextureImpl : public Direct3DRMObjectBase<IDirect3DRMTexture2> {
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override;
	HRESULT Changed(BOOL pixels, BOOL palette) override;
};
