#pragma once

#include "d3drm_impl.h"
#include "d3drmobject_impl.h"
#include "miniwin.h"

struct Direct3DRMLightImpl : public Direct3DRMObjectBaseImpl<IDirect3DRMLight> {
	Direct3DRMLightImpl(D3DRMLIGHTTYPE type, float r, float g, float b);
	HRESULT SetColorRGB(float r, float g, float b) override;
	D3DRMLIGHTTYPE GetType() override;
	D3DCOLOR GetColor() override;

private:
	D3DRMLIGHTTYPE m_type;
	D3DCOLOR m_color = 0xFFFFFFFF;
};

struct Direct3DRMLightArrayImpl
	: public Direct3DRMArrayBaseImpl<IDirect3DRMLight, Direct3DRMLightImpl, IDirect3DRMLightArray> {
	using Direct3DRMArrayBaseImpl::Direct3DRMArrayBaseImpl;
};
