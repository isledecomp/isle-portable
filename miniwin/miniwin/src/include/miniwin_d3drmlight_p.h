#pragma once

#include "miniwin_d3drmobject_p.h"

struct Direct3DRMLightImpl : public Direct3DRMObjectBase<IDirect3DRMLight> {
	HRESULT SetColorRGB(float r, float g, float b) override;
};
