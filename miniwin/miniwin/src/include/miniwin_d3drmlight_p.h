#pragma once

#include "miniwin_d3drm_p.h"
#include "miniwin_d3drmobject_p.h"
#include "miniwin_p.h"

struct Direct3DRMLightImpl : public Direct3DRMObjectBase<IDirect3DRMLight> {
	Direct3DRMLightImpl(float r, float g, float b);
	HRESULT SetColorRGB(float r, float g, float b) override;

private:
	D3DCOLOR m_color = 0xFFFFFFFF;
};

struct Direct3DRMLightArrayImpl
	: public Direct3DRMArrayBase<IDirect3DRMLight, Direct3DRMLightImpl, IDirect3DRMLightArray> {
	using Direct3DRMArrayBase::Direct3DRMArrayBase;
};
