#include "miniwin_d3drmframe_p.h"
#include "miniwin_p.h"

Direct3DRMFrameImpl::Direct3DRMFrameImpl()
{
	m_children = new Direct3DRMFrameArrayImpl;
	m_children->AddRef();
	m_lights = new Direct3DRMLightArrayImpl;
	m_lights->AddRef();
	m_visuals = new Direct3DRMVisualArrayImpl;
	m_visuals->AddRef();
}

Direct3DRMFrameImpl::~Direct3DRMFrameImpl()
{
	m_children->Release();
	m_lights->Release();
	m_visuals->Release();
	if (m_texture) {
		m_texture->Release();
	}
}

HRESULT Direct3DRMFrameImpl::AddChild(IDirect3DRMFrame* child)
{
	return m_children->AddElement(child);
}

HRESULT Direct3DRMFrameImpl::DeleteChild(IDirect3DRMFrame* child)
{
	return m_children->DeleteElement(child);
}

HRESULT Direct3DRMFrameImpl::SetSceneBackgroundRGB(float r, float g, float b)
{
	m_backgroundColor = (0xFF << 24) | (static_cast<BYTE>(r * 255.0f) << 16) | (static_cast<BYTE>(g * 255.0f) << 8) |
						(static_cast<BYTE>(b * 255.0f));
	return DD_OK;
}

HRESULT Direct3DRMFrameImpl::AddLight(IDirect3DRMLight* light)
{
	return m_lights->AddElement(light);
}

HRESULT Direct3DRMFrameImpl::GetLights(IDirect3DRMLightArray** lightArray)
{
	*lightArray = m_lights;
	m_lights->AddRef();
	return DD_OK;
}

HRESULT Direct3DRMFrameImpl::AddTransform(D3DRMCOMBINETYPE combine, D3DRMMATRIX4D matrix)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMFrameImpl::GetPosition(int index, D3DVECTOR* position)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMFrameImpl::AddVisual(IDirect3DRMVisual* visual)
{
	return m_visuals->AddElement(visual);
}

HRESULT Direct3DRMFrameImpl::DeleteVisual(IDirect3DRMVisual* visual)
{
	return m_visuals->DeleteElement(visual);
}

HRESULT Direct3DRMFrameImpl::GetVisuals(IDirect3DRMVisualArray** visuals)
{
	*visuals = m_visuals;
	m_visuals->AddRef();
	return DD_OK;
}

HRESULT Direct3DRMFrameImpl::SetTexture(IDirect3DRMTexture* texture)
{
	if (m_texture) {
		m_texture->Release();
	}

	m_texture = texture;
	m_texture->AddRef();
	return DD_OK;
}

HRESULT Direct3DRMFrameImpl::GetTexture(IDirect3DRMTexture** texture)
{
	if (!m_texture) {
		return DDERR_GENERIC;
	}

	*texture = m_texture;
	m_texture->AddRef();
	return DD_OK;
}

HRESULT Direct3DRMFrameImpl::SetColor(float r, float g, float b, float a)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMFrameImpl::SetColor(D3DCOLOR)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMFrameImpl::SetColorRGB(float r, float g, float b)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMFrameImpl::SetMaterialMode(D3DRMMATERIALMODE mode)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMFrameImpl::GetChildren(IDirect3DRMFrameArray** children)
{
	*children = m_children;
	m_children->AddRef();
	return DD_OK;
}
