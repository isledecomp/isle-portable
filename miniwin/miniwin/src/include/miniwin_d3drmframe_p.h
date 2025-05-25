#pragma once

#include "miniwin_d3drm_p.h"
#include "miniwin_d3drmobject_p.h"

struct Direct3DRMFrameImpl : public Direct3DRMObjectBase<IDirect3DRMFrame2> {
	Direct3DRMFrameImpl();
	~Direct3DRMFrameImpl() override;
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override;
	HRESULT AddChild(IDirect3DRMFrame* child) override;
	HRESULT DeleteChild(IDirect3DRMFrame* child) override;
	HRESULT SetSceneBackgroundRGB(float r, float g, float b) override;
	HRESULT AddLight(IDirect3DRMLight* light) override;
	HRESULT GetLights(IDirect3DRMLightArray** lightArray) override;
	HRESULT AddTransform(D3DRMCOMBINETYPE combine, D3DRMMATRIX4D matrix) override;
	HRESULT GetPosition(int index, D3DVECTOR* position) override;
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

	D3DCOLOR m_backgroundColor = 0xFF000000;

private:
	IDirect3DRMFrameArray* m_children;
	IDirect3DRMLightArray* m_lights;
	IDirect3DRMVisualArray* m_visuals;
	IDirect3DRMTexture* m_texture = nullptr;
};
