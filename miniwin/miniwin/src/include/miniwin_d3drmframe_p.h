#pragma once

#include "miniwin_d3drmobject_p.h"

class Direct3DRMTextureImpl;
class Direct3DRMLightArrayImpl;
class Direct3DRMVisualArrayImpl;
class Direct3DRMFrameArrayImpl;

struct Direct3DRMFrameImpl : public Direct3DRMObjectBase<IDirect3DRMFrame2> {
	Direct3DRMFrameImpl(Direct3DRMFrameImpl* parent = nullptr);
	~Direct3DRMFrameImpl() override;
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override;
	HRESULT AddChild(IDirect3DRMFrame* child) override;
	HRESULT DeleteChild(IDirect3DRMFrame* child) override;
	HRESULT SetSceneBackgroundRGB(float r, float g, float b) override;
	HRESULT AddLight(IDirect3DRMLight* light) override;
	HRESULT GetLights(IDirect3DRMLightArray** lightArray) override;
	HRESULT AddTransform(D3DRMCOMBINETYPE combine, D3DRMMATRIX4D matrix) override;
	HRESULT GetPosition(IDirect3DRMFrame* reference, D3DVECTOR* position) override;
	HRESULT AddVisual(IDirect3DRMVisual* visual) override;
	HRESULT DeleteVisual(IDirect3DRMVisual* visual) override;
	HRESULT AddVisual(IDirect3DRMMesh* visual) override;
	HRESULT DeleteVisual(IDirect3DRMMesh* visual) override;
	HRESULT AddVisual(IDirect3DRMFrame* visual) override;
	HRESULT DeleteVisual(IDirect3DRMFrame* visual) override;
	HRESULT GetVisuals(IDirect3DRMVisualArray** visuals) override;
	HRESULT SetTexture(IDirect3DRMTexture* texture) override;
	HRESULT GetTexture(IDirect3DRMTexture** texture) override;
	HRESULT SetColor(float r, float g, float b, float a) override;
	HRESULT SetColor(D3DCOLOR) override;
	HRESULT SetColorRGB(float r, float g, float b) override;
	HRESULT SetMaterialMode(D3DRMMATERIALMODE mode) override;
	HRESULT GetChildren(IDirect3DRMFrameArray** children) override;

private:
	Direct3DRMFrameImpl* m_parent{};
	Direct3DRMFrameArrayImpl* m_children{};
	Direct3DRMLightArrayImpl* m_lights{};
	Direct3DRMVisualArrayImpl* m_visuals{};
	Direct3DRMTextureImpl* m_texture{};
	D3DRMMATRIX4D m_transform =
		{{1.f, 0.f, 0.f, 0.f}, {0.f, 1.f, 0.f, 0.f}, {0.f, 0.f, 1.f, 0.f}, {0.f, 0.f, 0.f, 1.f}};

	D3DCOLOR m_backgroundColor = 0xFF000000;
	D3DCOLOR m_color = 0xffffff;

	friend class Direct3DRMViewportImpl;
};

struct Direct3DRMFrameArrayImpl
	: public Direct3DRMArrayBase<IDirect3DRMFrame, Direct3DRMFrameImpl, IDirect3DRMFrameArray> {
	using Direct3DRMArrayBase::Direct3DRMArrayBase;

	friend class Direct3DRMFrameImpl;
};
