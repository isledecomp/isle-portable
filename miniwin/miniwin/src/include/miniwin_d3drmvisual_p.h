#pragma once

#include "miniwin_d3drm.h"
#include "miniwin_d3drmobject_p.h"

struct Direct3DRMVisualArrayImpl
	: public Direct3DRMArrayBase<IDirect3DRMVisual, IDirect3DRMVisual, IDirect3DRMVisualArray> {
	using Direct3DRMArrayBase::Direct3DRMArrayBase;
};
