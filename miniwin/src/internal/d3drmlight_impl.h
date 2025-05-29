#pragma once

#include "d3drm_impl.h"
#include "d3drmobject_impl.h"
#include "miniwin.h"

struct Direct3DRMLightImpl : public Direct3DRMObjectBaseImpl<IDirect3DRMLight> {
	Direct3DRMLightImpl(float r, float g, float b);
	HRESULT SetColorRGB(float r, float g, float b) override;

private:
	D3DCOLOR m_color = 0xFFFFFFFF;
};

struct Direct3DRMLightArrayImpl
	: public Direct3DRMArrayBaseImpl<IDirect3DRMLight, Direct3DRMLightImpl, IDirect3DRMLightArray> {
	using Direct3DRMArrayBaseImpl::Direct3DRMArrayBaseImpl;
};
