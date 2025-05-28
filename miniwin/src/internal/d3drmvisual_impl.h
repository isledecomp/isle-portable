#pragma once

#include "d3drmobject_impl.h"
#include "miniwin/d3drm.h"

struct Direct3DRMVisualArrayImpl
	: public Direct3DRMArrayBaseImpl<IDirect3DRMVisual, IDirect3DRMVisual, IDirect3DRMVisualArray> {
	using Direct3DRMArrayBaseImpl::Direct3DRMArrayBaseImpl;
};
