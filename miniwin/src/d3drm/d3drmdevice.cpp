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
	SDL_GetWindowSizeInPixels(DDWindow, &m_windowWidth, &m_windowHeight);
#ifdef __3DS__
	m_windowWidth = 320; // We are on the lower screen
	m_windowHeight = 240;
#endif
	m_viewportTransform = CalculateViewportTransform(m_virtualWidth, m_virtualHeight, m_windowWidth, m_windowHeight);
	m_renderer->Resize(m_windowWidth, m_windowHeight, m_viewportTransform);
	m_renderer->Clear(0, 0, 0);
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
	case SDL_EVENT_MOUSE_MOTION: {
		event->motion.x = (event->motion.x - m_viewportTransform.offsetX) / m_viewportTransform.scale;
		event->motion.y = (event->motion.y - m_viewportTransform.offsetY) / m_viewportTransform.scale;
		break;
	}
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP: {
		event->button.x = (event->button.x - m_viewportTransform.offsetX) / m_viewportTransform.scale;
		event->button.y = (event->button.y - m_viewportTransform.offsetY) / m_viewportTransform.scale;
		break;
	}
	case SDL_EVENT_FINGER_MOTION:
	case SDL_EVENT_FINGER_DOWN:
	case SDL_EVENT_FINGER_UP:
	case SDL_EVENT_FINGER_CANCELED: {
		float x = (event->tfinger.x * m_windowWidth - m_viewportTransform.offsetX) / m_viewportTransform.scale;
		float y = (event->tfinger.y * m_windowHeight - m_viewportTransform.offsetY) / m_viewportTransform.scale;
		event->tfinger.x = x / m_virtualWidth;
		event->tfinger.y = y / m_virtualHeight;
		break;
	}
	}

	return true;
}

bool Direct3DRMDevice2Impl::ConvertRenderToWindowCoordinates(Sint32 inX, Sint32 inY, Sint32& outX, Sint32& outY)
{
	outX = static_cast<Sint32>(inX * m_viewportTransform.scale + m_viewportTransform.offsetX);
	outY = static_cast<Sint32>(inY * m_viewportTransform.scale + m_viewportTransform.offsetY);

	return true;
}
