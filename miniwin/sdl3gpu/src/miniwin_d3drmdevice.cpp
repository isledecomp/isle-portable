#include "miniwin_d3drm.h"
#include "miniwin_d3drm_sdl3gpu.h"
#include "miniwin_d3drmdevice_sdl3gpu.h"
#include "miniwin_d3drmobject_sdl3gpu.h"
#include "miniwin_d3drmviewport_sdl3gpu.h"
#include "miniwin_p.h"

#include <SDL3/SDL.h>

Direct3DRMDevice2_SDL3GPUImpl::Direct3DRMDevice2_SDL3GPUImpl(DWORD width, DWORD height, SDL_GPUDevice* device)
	: m_width(width), m_height(height), m_device(device), m_viewports(new Direct3DRMViewportArray_SDL3GPUImpl)
{
}

Direct3DRMDevice2_SDL3GPUImpl::~Direct3DRMDevice2_SDL3GPUImpl()
{
	for (int i = 0; i < m_viewports->GetSize(); i++) {
		IDirect3DRMViewport* viewport;
		m_viewports->GetElement(i, &viewport);
		static_cast<Direct3DRMViewport_SDL3GPUImpl*>(viewport)->CloseDevice();
		viewport->Release();
	}
	m_viewports->Release();

	SDL_ReleaseWindowFromGPUDevice(m_device, DDWindow);
	SDL_DestroyGPUDevice(m_device);
}

DWORD Direct3DRMDevice2_SDL3GPUImpl::GetWidth()
{
	return m_width;
}

DWORD Direct3DRMDevice2_SDL3GPUImpl::GetHeight()
{
	return m_height;
}

HRESULT Direct3DRMDevice2_SDL3GPUImpl::SetBufferCount(int count)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

DWORD Direct3DRMDevice2_SDL3GPUImpl::GetBufferCount()
{
	MINIWIN_NOT_IMPLEMENTED();
	return 2;
}

HRESULT Direct3DRMDevice2_SDL3GPUImpl::SetShades(DWORD shadeCount)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMDevice2_SDL3GPUImpl::GetShades()
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMDevice2_SDL3GPUImpl::SetQuality(D3DRMRENDERQUALITY quality)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

D3DRMRENDERQUALITY Direct3DRMDevice2_SDL3GPUImpl::GetQuality()
{
	MINIWIN_NOT_IMPLEMENTED();
	return D3DRMRENDERQUALITY::GOURAUD;
}

HRESULT Direct3DRMDevice2_SDL3GPUImpl::SetDither(BOOL dither)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

BOOL Direct3DRMDevice2_SDL3GPUImpl::GetDither()
{
	MINIWIN_NOT_IMPLEMENTED();
	return false;
}

HRESULT Direct3DRMDevice2_SDL3GPUImpl::SetTextureQuality(D3DRMTEXTUREQUALITY quality)
{
	return DD_OK;
}

D3DRMTEXTUREQUALITY Direct3DRMDevice2_SDL3GPUImpl::GetTextureQuality()
{
	return D3DRMTEXTUREQUALITY::LINEAR;
}

HRESULT Direct3DRMDevice2_SDL3GPUImpl::SetRenderMode(D3DRMRENDERMODE mode)
{
	return DD_OK;
}

D3DRMRENDERMODE Direct3DRMDevice2_SDL3GPUImpl::GetRenderMode()
{
	return D3DRMRENDERMODE::BLENDEDTRANSPARENCY;
}

HRESULT Direct3DRMDevice2_SDL3GPUImpl::Update()
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMDevice2_SDL3GPUImpl::AddViewport(IDirect3DRMViewport* viewport)
{
	return m_viewports->AddElement(viewport);
}

HRESULT Direct3DRMDevice2_SDL3GPUImpl::GetViewports(IDirect3DRMViewportArray** ppViewportArray)
{
	m_viewports->AddRef();
	*ppViewportArray = m_viewports;
	return DD_OK;
}
