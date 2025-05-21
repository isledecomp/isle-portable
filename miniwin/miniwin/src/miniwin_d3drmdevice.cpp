#include "miniwin_d3drm.h"
#include "miniwin_d3drm_p.h"
#include "miniwin_d3drmobject_p.h"
#include "miniwin_d3drmviewport_p.h"
#include "miniwin_p.h"

#include <SDL3/SDL.h>

Direct3DRMDevice2Impl::Direct3DRMDevice2Impl(DWORD width, DWORD height, SDL_GPUDevice* device)
	: m_width(width), m_height(height), m_device(device), m_viewports(new Direct3DRMViewportArrayImpl)
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

	SDL_ReleaseWindowFromGPUDevice(m_device, DDWindow);
	SDL_DestroyGPUDevice(m_device);
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
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMDevice2Impl::GetShades()
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMDevice2Impl::SetQuality(D3DRMRENDERQUALITY quality)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

D3DRMRENDERQUALITY Direct3DRMDevice2Impl::GetQuality()
{
	MINIWIN_NOT_IMPLEMENTED();
	return D3DRMRENDERQUALITY::GOURAUD;
}

HRESULT Direct3DRMDevice2Impl::SetDither(int dither)
{
	MINIWIN_NOT_IMPLEMENTED();
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
	for (int i = 0; i < m_viewports->GetSize(); i++) {
		IDirect3DRMViewport* viewport;
		m_viewports->GetElement(i, &viewport);
		static_cast<Direct3DRMViewportImpl*>(viewport)->Update();
	}

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
