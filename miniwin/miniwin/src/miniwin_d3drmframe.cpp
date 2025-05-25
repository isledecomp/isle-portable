#include "miniwin_d3drm_p.h"
#include "miniwin_d3drmframe_p.h"
#include "miniwin_d3drmlight_p.h"
#include "miniwin_d3drmtexture_p.h"
#include "miniwin_d3drmvisual_p.h"
#include "miniwin_p.h"

#include <cstring>

Direct3DRMFrameImpl::Direct3DRMFrameImpl(Direct3DRMFrameImpl* parent)
{
	m_children = new Direct3DRMFrameArrayImpl;
	m_children->AddRef();
	m_lights = new Direct3DRMLightArrayImpl;
	m_lights->AddRef();
	m_visuals = new Direct3DRMVisualArrayImpl;
	m_visuals->AddRef();
	if (parent) {
		parent->AddChild(this);
	}
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

HRESULT Direct3DRMFrameImpl::QueryInterface(const GUID& riid, void** ppvObject)
{
	if (SDL_memcmp(&riid, &IID_IDirect3DRMFrame, sizeof(GUID)) == 0) {
		this->IUnknown::AddRef();
		*ppvObject = static_cast<IDirect3DRMFrame*>(this);
		return S_OK;
	}
	if (SDL_memcmp(&riid, &IID_IDirect3DRMMesh, sizeof(GUID)) == 0) {
		return E_NOINTERFACE;
	}
	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Direct3DRMFrameImpl does not implement guid");
	return E_NOINTERFACE;
}

HRESULT Direct3DRMFrameImpl::AddChild(IDirect3DRMFrame* child)
{
	if (!child) {
		return DDERR_GENERIC;
	}
	Direct3DRMFrameImpl* childImpl = static_cast<Direct3DRMFrameImpl*>(child);
	if (childImpl->m_parent) {
		if (childImpl->m_parent == this) {
			return DD_OK;
		}
		auto result = childImpl->m_parent->m_children->DeleteElement(childImpl);
		SDL_assert(result == DD_OK);
	}
	childImpl->m_parent = this;
	return m_children->AddElement(child);
}

HRESULT Direct3DRMFrameImpl::DeleteChild(IDirect3DRMFrame* child)
{
	Direct3DRMFrameImpl* childImpl = static_cast<Direct3DRMFrameImpl*>(child);
	if (!childImpl) {
		return DDERR_GENERIC;
	}
	HRESULT result = m_children->DeleteElement(childImpl);
	if (result == DD_OK) {
		childImpl->m_parent = nullptr;
	}
	return result;
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
	switch (combine) {
	case D3DRMCOMBINETYPE::REPLACE:
		std::memcpy(m_transform, matrix, sizeof(m_transform));
		return DD_OK;
	default:
		MINIWIN_NOT_IMPLEMENTED();
		return DDERR_GENERIC;
	}
}

HRESULT Direct3DRMFrameImpl::GetPosition(IDirect3DRMFrame* reference, D3DVECTOR* position)
{
	if (reference) {
		MINIWIN_NOT_IMPLEMENTED();
		return DDERR_GENERIC;
	}
	position->x = m_transform[3][0] / m_transform[3][3];
	position->y = m_transform[3][1] / m_transform[3][3];
	position->z = m_transform[3][2] / m_transform[3][3];
	return DD_OK;
}

HRESULT Direct3DRMFrameImpl::AddVisual(IDirect3DRMVisual* visual)
{
	SDL_assert(false); // Is this actually used?
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
	if (!texture) {
		return DDERR_GENERIC;
	}
	auto textureImpl = static_cast<Direct3DRMTextureImpl*>(texture);
	if (m_texture) {
		m_texture->Release();
	}

	m_texture = textureImpl;
	m_texture->AddRef();
	return DD_OK;
}

HRESULT Direct3DRMFrameImpl::GetTexture(IDirect3DRMTexture** texture)
{
	if (!m_texture) {
		return DDERR_GENERIC;
	}

	*texture = m_texture;
	if (m_texture) {
		m_texture->AddRef();
	}
	return DD_OK;
}

HRESULT Direct3DRMFrameImpl::SetColor(float r, float g, float b, float a)
{
	m_color = (static_cast<BYTE>(a * 255.0f) << 24) | (static_cast<BYTE>(r * 255.0f) << 16) |
			  (static_cast<BYTE>(g * 255.0f) << 8) | (static_cast<BYTE>(b * 255.0f));
	return DD_OK;
}

HRESULT Direct3DRMFrameImpl::SetColor(D3DCOLOR c)
{
	m_color = c;
	return DD_OK;
}

HRESULT Direct3DRMFrameImpl::SetColorRGB(float r, float g, float b)
{
	return SetColor(r, g, b, 1.f);
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
