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
	: m_virtualWidth(width), m_virtualHeight(height), m_renderer(renderer), m_viewports(new Direct3DRMViewportArrayImpl)
{
	Resize();
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
	m_renderer->SetDither(dither);
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
	return DD_OK;
}

HRESULT Direct3DRMDevice2Impl::AddViewport(IDirect3DRMViewport* viewport)
{
	HRESULT status = m_viewports->AddElement(viewport);
	Resize();
	return status;
}

HRESULT Direct3DRMDevice2Impl::GetViewports(IDirect3DRMViewportArray** ppViewportArray)
{
	m_viewports->AddRef();
	*ppViewportArray = m_viewports;
	return DD_OK;
}

ViewportTransform CalculateViewportTransform(int virtualW, int virtualH, int windowW, int windowH)
{
	float scaleX = (float) windowW / virtualW;
	float scaleY = (float) windowH / virtualH;
	float scale = (scaleX < scaleY) ? scaleX : scaleY;

	float viewportW = virtualW * scale;
	float viewportH = virtualH * scale;

	float offsetX = (windowW - viewportW) * 0.5f;
	float offsetY = (windowH - viewportH) * 0.5f;

	return {scale, offsetX, offsetY};
}

void Direct3DRMDevice2Impl::Resize()
{
	int width, height;
	SDL_GetWindowSizeInPixels(DDWindow, &width, &height);
#ifdef __3DS__
	width = 320; // We are on the lower screen
	height = 240;
#endif
	m_viewportTransform = CalculateViewportTransform(m_virtualWidth, m_virtualHeight, width, height);
	m_renderer->Resize(width, height, m_viewportTransform);
	for (int i = 0; i < m_viewports->GetSize(); i++) {
		IDirect3DRMViewport* viewport;
		m_viewports->GetElement(i, &viewport);
		static_cast<Direct3DRMViewportImpl*>(viewport)->UpdateProjectionMatrix();
	}
}

bool Direct3DRMDevice2Impl::ConvertEventToRenderCoordinates(SDL_Event* event)
{
	switch (event->type) {
	case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
		Resize();
		break;
	}
	case SDL_EVENT_MOUSE_MOTION:
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP: {
		int rawX = event->motion.x;
		int rawY = event->motion.y;
		float x = (rawX - m_viewportTransform.offsetX) / m_viewportTransform.scale;
		float y = (rawY - m_viewportTransform.offsetY) / m_viewportTransform.scale;
		event->motion.x = static_cast<Sint32>(x);
		event->motion.y = static_cast<Sint32>(y);
		break;
	} break;
	}

	return true;
}
