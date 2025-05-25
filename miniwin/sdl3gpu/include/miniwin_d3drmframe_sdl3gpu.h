#pragma once

#include "miniwin_d3drmobject_sdl3gpu.h"

class Direct3DRMTexture_SDL3GPUImpl;
class Direct3DRMLightArray_SDL3GPUImpl;
class Direct3DRMVisualArray_SDL3GPUImpl;
class Direct3DRMFrameArray_SDL3GPUImpl;

struct Direct3DRMFrame_SDL3GPUImpl : public Direct3DRMObjectBase_SDL3GPUImpl<IDirect3DRMFrame2> {
	Direct3DRMFrame_SDL3GPUImpl(Direct3DRMFrame_SDL3GPUImpl* parent = nullptr);
	~Direct3DRMFrame_SDL3GPUImpl() override;
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
	HRESULT GetVisuals(IDirect3DRMVisualArray** visuals) override;
	HRESULT SetTexture(IDirect3DRMTexture* texture) override;
	HRESULT GetTexture(IDirect3DRMTexture** texture) override;
	HRESULT SetColor(float r, float g, float b, float a) override;
	HRESULT SetColor(D3DCOLOR) override;
	HRESULT SetColorRGB(float r, float g, float b) override;
	HRESULT SetMaterialMode(D3DRMMATERIALMODE mode) override;
	HRESULT GetChildren(IDirect3DRMFrameArray** children) override;

	Direct3DRMFrame_SDL3GPUImpl* m_parent{};
	D3DRMMATRIX4D m_transform =
		{{1.f, 0.f, 0.f, 0.f}, {0.f, 1.f, 0.f, 0.f}, {0.f, 0.f, 1.f, 0.f}, {0.f, 0.f, 0.f, 1.f}};

private:
	Direct3DRMFrameArray_SDL3GPUImpl* m_children{};
	Direct3DRMLightArray_SDL3GPUImpl* m_lights{};
	Direct3DRMVisualArray_SDL3GPUImpl* m_visuals{};
	Direct3DRMTexture_SDL3GPUImpl* m_texture{};

	D3DCOLOR m_backgroundColor = 0xFF000000;
	D3DCOLOR m_color = 0xffffff;

	friend class Direct3DRMViewport_SDL3GPUImpl;
};

struct Direct3DRMFrameArray_SDL3GPUImpl
	: public Direct3DRMArrayBase_SDL3GPUImpl<IDirect3DRMFrame, Direct3DRMFrame_SDL3GPUImpl, IDirect3DRMFrameArray> {
	using Direct3DRMArrayBase_SDL3GPUImpl::Direct3DRMArrayBase_SDL3GPUImpl;

	friend class Direct3DRMFrame_SDL3GPUImpl;
};
