#include "d3drm_impl.h"
#include "d3drmdevice_impl.h"
#include "d3drmobject_impl.h"
#include "d3drmrenderer.h"
#include "d3drmviewport_impl.h"
#include "ddraw_impl.h"
#include "miniwin.h"
#include "miniwin/d3drm.h"
#include "miniwin/miniwindevice.h"

#include <SDL3/SDL.h>

Direct3DRMDevice2Impl::Direct3DRMDevice2Impl(DWORD width, DWORD height, Direct3DRMRenderer* renderer)
	: m_width(width), m_height(height), m_renderer(renderer), m_viewports(new Direct3DRMViewportArrayImpl)
{
}

Direct3DRMDevice2Impl::~Direct3DRMDevice2Impl()
{
	for (int i = 0; i < m_viewports->GetSize(); i++) {
		IDirect3DRMViewport* viewport;
		m_viewports->GetElement(i, &viewport);
		static_cast<Direct3DRMViewportImpl*>(viewport)->CloseDevice();
		viewport->Release();
	}
	m_viewports->Release();
	delete m_renderer;
}

HRESULT Direct3DRMDevice2Impl::QueryInterface(const GUID& riid, void** ppvObject)
{
	if (SDL_memcmp(&riid, &IID_IDirect3DRMDevice2, sizeof(riid)) == 0) {
		this->IUnknown::AddRef();
		*ppvObject = static_cast<IDirect3DRMDevice2*>(this);
		return DD_OK;
	}
	else if (SDL_memcmp(&riid, &IID_IDirect3DRMMiniwinDevice, sizeof(riid)) == 0) {
		this->IUnknown::AddRef();
		*ppvObject = static_cast<IDirect3DRMMiniwinDevice*>(this);
		return DD_OK;
	}
	MINIWIN_NOT_IMPLEMENTED();
	return E_NOINTERFACE;
}

DWORD Direct3DRMDevice2Impl::GetWidth()
{
	return m_width;
}

DWORD Direct3DRMDevice2Impl::GetHeight()
{
	return m_height;
}

HRESULT Direct3DRMDevice2Impl::SetBufferCount(int count)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

DWORD Direct3DRMDevice2Impl::GetBufferCount()
{
	MINIWIN_NOT_IMPLEMENTED();
	return 2;
}

HRESULT Direct3DRMDevice2Impl::SetShades(DWORD shadeCount)
{
	if (shadeCount != 256) {
		MINIWIN_NOT_IMPLEMENTED();
	}
	return DD_OK;
}

DWORD Direct3DRMDevice2Impl::GetShades()
{
	return 256;
}

HRESULT Direct3DRMDevice2Impl::SetQuality(D3DRMRENDERQUALITY quality)
{
	if (quality != D3DRMRENDER_GOURAUD && quality != D3DRMRENDER_PHONG) {
		MINIWIN_NOT_IMPLEMENTED();
	}
	return DD_OK;
}

D3DRMRENDERQUALITY Direct3DRMDevice2Impl::GetQuality()
{
	return D3DRMRENDERQUALITY::GOURAUD;
}

HRESULT Direct3DRMDevice2Impl::SetDither(BOOL dither)
{
	if (dither) {
		MINIWIN_NOT_IMPLEMENTED();
	}
	return DD_OK;
}

BOOL Direct3DRMDevice2Impl::GetDither()
{
	MINIWIN_NOT_IMPLEMENTED();
	return false;
}

HRESULT Direct3DRMDevice2Impl::SetTextureQuality(D3DRMTEXTUREQUALITY quality)
{
	return DD_OK;
}

D3DRMTEXTUREQUALITY Direct3DRMDevice2Impl::GetTextureQuality()
{
	return D3DRMTEXTUREQUALITY::LINEAR;
}

HRESULT Direct3DRMDevice2Impl::SetRenderMode(D3DRMRENDERMODE mode)
{
	return DD_OK;
}

D3DRMRENDERMODE Direct3DRMDevice2Impl::GetRenderMode()
{
	return D3DRMRENDERMODE::BLENDEDTRANSPARENCY;
}

HRESULT Direct3DRMDevice2Impl::Update()
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMDevice2Impl::AddViewport(IDirect3DRMViewport* viewport)
{
	return m_viewports->AddElement(viewport);
}

HRESULT Direct3DRMDevice2Impl::GetViewports(IDirect3DRMViewportArray** ppViewportArray)
{
	m_viewports->AddRef();
	*ppViewportArray = m_viewports;
	return DD_OK;
}

float Direct3DRMDevice2Impl::GetShininessFactor()
{
	return m_renderer->GetShininessFactor();
}

bool Direct3DRMDevice2Impl::ConvertEventToRenderCoordinates(SDL_Event* event)
{
	return m_renderer->ConvertEventToRenderCoordinates(event);
}

HRESULT Direct3DRMDevice2Impl::SetShininessFactor(float factor)
{
	return m_renderer->SetShininessFactor(factor);
}
