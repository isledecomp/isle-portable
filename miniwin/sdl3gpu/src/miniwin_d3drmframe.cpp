#include "miniwin_d3drm_sdl3gpu.h"
#include "miniwin_d3drmframe_sdl3gpu.h"
#include "miniwin_d3drmlight_sdl3gpu.h"
#include "miniwin_d3drmtexture_sdl3gpu.h"
#include "miniwin_d3drmvisual_sdl3gpu.h"
#include "miniwin_p.h"

#include <cstring>

Direct3DRMFrame_SDL3GPUImpl::Direct3DRMFrame_SDL3GPUImpl(Direct3DRMFrame_SDL3GPUImpl* parent)
{
	m_children = new Direct3DRMFrameArray_SDL3GPUImpl;
	m_children->AddRef();
	m_lights = new Direct3DRMLightArray_SDL3GPUImpl;
	m_lights->AddRef();
	m_visuals = new Direct3DRMVisualArray_SDL3GPUImpl;
	m_visuals->AddRef();
	if (parent) {
		parent->AddChild(this);
	}
}

Direct3DRMFrame_SDL3GPUImpl::~Direct3DRMFrame_SDL3GPUImpl()
{
	m_children->Release();
	m_lights->Release();
	m_visuals->Release();
	if (m_texture) {
		m_texture->Release();
	}
}

HRESULT Direct3DRMFrame_SDL3GPUImpl::QueryInterface(const GUID& riid, void** ppvObject)
{
	if (SDL_memcmp(&riid, &IID_IDirect3DRMFrame, sizeof(GUID)) == 0) {
		this->IUnknown::AddRef();
		*ppvObject = static_cast<IDirect3DRMFrame*>(this);
		return S_OK;
	}
	if (SDL_memcmp(&riid, &IID_IDirect3DRMMesh, sizeof(GUID)) == 0) {
		return E_NOINTERFACE;
	}
	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Direct3DRMFrame_SDL3GPUImpl does not implement guid");
	return E_NOINTERFACE;
}

HRESULT Direct3DRMFrame_SDL3GPUImpl::AddChild(IDirect3DRMFrame* child)
{
	Direct3DRMFrame_SDL3GPUImpl* childImpl = static_cast<Direct3DRMFrame_SDL3GPUImpl*>(child);
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

HRESULT Direct3DRMFrame_SDL3GPUImpl::DeleteChild(IDirect3DRMFrame* child)
{
	Direct3DRMFrame_SDL3GPUImpl* childImpl = static_cast<Direct3DRMFrame_SDL3GPUImpl*>(child);
	HRESULT result = m_children->DeleteElement(childImpl);
	if (result == DD_OK) {
		childImpl->m_parent = nullptr;
	}
	return result;
}

HRESULT Direct3DRMFrame_SDL3GPUImpl::SetSceneBackgroundRGB(float r, float g, float b)
{
	m_backgroundColor = (0xFF << 24) | (static_cast<BYTE>(r * 255.0f) << 16) | (static_cast<BYTE>(g * 255.0f) << 8) |
						(static_cast<BYTE>(b * 255.0f));
	return DD_OK;
}

HRESULT Direct3DRMFrame_SDL3GPUImpl::AddLight(IDirect3DRMLight* light)
{
	return m_lights->AddElement(light);
}

HRESULT Direct3DRMFrame_SDL3GPUImpl::GetLights(IDirect3DRMLightArray** lightArray)
{
	*lightArray = m_lights;
	m_lights->AddRef();
	return DD_OK;
}

HRESULT Direct3DRMFrame_SDL3GPUImpl::AddTransform(D3DRMCOMBINETYPE combine, D3DRMMATRIX4D matrix)
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

HRESULT Direct3DRMFrame_SDL3GPUImpl::GetPosition(IDirect3DRMFrame* reference, D3DVECTOR* position)
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

HRESULT Direct3DRMFrame_SDL3GPUImpl::AddVisual(IDirect3DRMVisual* visual)
{
	return m_visuals->AddElement(visual);
}

HRESULT Direct3DRMFrame_SDL3GPUImpl::DeleteVisual(IDirect3DRMVisual* visual)
{
	return m_visuals->DeleteElement(visual);
}

HRESULT Direct3DRMFrame_SDL3GPUImpl::GetVisuals(IDirect3DRMVisualArray** visuals)
{
	*visuals = m_visuals;
	m_visuals->AddRef();
	return DD_OK;
}

HRESULT Direct3DRMFrame_SDL3GPUImpl::SetTexture(IDirect3DRMTexture* texture)
{
	auto textureImpl = static_cast<Direct3DRMTexture_SDL3GPUImpl*>(texture);
	if (m_texture) {
		m_texture->Release();
	}

	m_texture = textureImpl;
	m_texture->AddRef();
	return DD_OK;
}

HRESULT Direct3DRMFrame_SDL3GPUImpl::GetTexture(IDirect3DRMTexture** texture)
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

HRESULT Direct3DRMFrame_SDL3GPUImpl::SetColor(float r, float g, float b, float a)
{
	m_color = (static_cast<BYTE>(a * 255.0f) << 24) | (static_cast<BYTE>(r * 255.0f) << 16) |
			  (static_cast<BYTE>(g * 255.0f) << 8) | (static_cast<BYTE>(b * 255.0f));
	return DD_OK;
}

HRESULT Direct3DRMFrame_SDL3GPUImpl::SetColor(D3DCOLOR c)
{
	m_color = c;
	return DD_OK;
}

HRESULT Direct3DRMFrame_SDL3GPUImpl::SetColorRGB(float r, float g, float b)
{
	return SetColor(r, g, b, 1.f);
}

HRESULT Direct3DRMFrame_SDL3GPUImpl::SetMaterialMode(D3DRMMATERIALMODE mode)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMFrame_SDL3GPUImpl::GetChildren(IDirect3DRMFrameArray** children)
{
	*children = m_children;
	m_children->AddRef();
	return DD_OK;
}
