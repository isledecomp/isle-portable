#pragma once

#include "miniwin_d3drmobject_sdl3gpu.h"

struct Direct3DRMTexture_SDL3GPUImpl : public Direct3DRMObjectBase_SDL3GPUImpl<IDirect3DRMTexture2> {
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override;
	HRESULT Changed(BOOL pixels, BOOL palette) override;
};
