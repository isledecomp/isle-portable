#pragma once

#include "miniwin_d3drm.h"
#include "miniwin_d3drmobject_sdl3gpu.h"

struct Direct3DRMVisualArray_SDL3GPUImpl
	: public Direct3DRMArrayBase_SDL3GPUImpl<IDirect3DRMVisual, IDirect3DRMVisual, IDirect3DRMVisualArray> {
	using Direct3DRMArrayBase_SDL3GPUImpl::Direct3DRMArrayBase_SDL3GPUImpl;
};
