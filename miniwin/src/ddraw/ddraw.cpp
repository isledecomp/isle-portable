#ifdef USE_OPENGL1
#include "d3drmrenderer_opengl1.h"
#endif
#ifdef USE_OPENGLES2
#include "d3drmrenderer_opengles2.h"
#endif
#ifdef _WIN32
#include "d3drmrenderer_directx9.h"
#endif
#include "d3drmrenderer_sdl3gpu.h"
#include "d3drmrenderer_software.h"
#include "ddpalette_impl.h"
#include "ddraw_impl.h"
#include "ddsurface_impl.h"
#include "dummysurface_impl.h"
#include "miniwin.h"
#include "miniwin/d3d.h"

#include <SDL3/SDL.h>
#include <assert.h>
#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <cstring>

SDL_Window* DDWindow;
Direct3DRMRenderer* DDRenderer;

HRESULT DirectDrawImpl::QueryInterface(const GUID& riid, void** ppvObject)
{
	if (SDL_memcmp(&riid, &IID_IDirectDraw2, sizeof(GUID)) == 0) {
		this->IUnknown::AddRef();
		*ppvObject = static_cast<IDirectDraw2*>(this);
		return S_OK;
	}
	if (SDL_memcmp(&riid, &IID_IDirect3D2, sizeof(GUID)) == 0) {
		this->IUnknown::AddRef();
		*ppvObject = static_cast<IDirect3D2*>(this);
		return S_OK;
	}
	MINIWIN_NOT_IMPLEMENTED();
	return E_NOINTERFACE;
}

// IDirectDraw interface
HRESULT DirectDrawImpl::CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER* lplpDDClipper, IUnknown* pUnkOuter)
{
	*lplpDDClipper = new IDirectDrawClipper;

	return DD_OK;
}

HRESULT DirectDrawImpl::CreatePalette(
	DDPixelCaps dwFlags,
	LPPALETTEENTRY lpColorTable,
	LPDIRECTDRAWPALETTE* lplpDDPalette,
	IUnknown* pUnkOuter
)
{
	if ((dwFlags & DDPCAPS_8BIT) != DDPCAPS_8BIT) {
		return DDERR_INVALIDPARAMS;
	}

	*lplpDDPalette = static_cast<LPDIRECTDRAWPALETTE>(new DirectDrawPaletteImpl(lpColorTable));
	return DD_OK;
}

HRESULT DirectDrawImpl::CreateSurface(
	LPDDSURFACEDESC lpDDSurfaceDesc,
	LPDIRECTDRAWSURFACE* lplpDDSurface,
	IUnknown* pUnkOuter
)
{
	if ((lpDDSurfaceDesc->dwFlags & DDSD_CAPS) == DDSD_CAPS) {
		if ((lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_ZBUFFER) == DDSCAPS_ZBUFFER) {
			if ((lpDDSurfaceDesc->dwFlags & DDSD_ZBUFFERBITDEPTH) != DDSD_ZBUFFERBITDEPTH) {
				return DDERR_INVALIDPARAMS;
			}
			SDL_Log("Todo: Set %" PRIu32 "bit Z-Buffer", lpDDSurfaceDesc->dwZBufferBitDepth);
			*lplpDDSurface = static_cast<IDirectDrawSurface*>(new DummySurfaceImpl);
			return DD_OK;
		}
		if ((lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) == DDSCAPS_PRIMARYSURFACE) {
			m_frameBuffer = new FrameBufferImpl(m_virtualWidth, m_virtualHeight);
			*lplpDDSurface = static_cast<IDirectDrawSurface*>(m_frameBuffer);
			return DD_OK;
		}
		if ((lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_3DDEVICE) == DDSCAPS_3DDEVICE) {
			m_frameBuffer->AddRef();
			*lplpDDSurface = static_cast<IDirectDrawSurface*>(m_frameBuffer);
			return DD_OK;
		}
		if ((lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_TEXTURE) == DDSCAPS_TEXTURE) {
			MINIWIN_TRACE("DDSCAPS_TEXTURE"); // Texture for use in 3D
		}
	}

	SDL_PixelFormat format;
#ifdef MINIWIN_PIXELFORMAT
	format = MINIWIN_PIXELFORMAT;
#else
	format = SDL_PIXELFORMAT_RGBA32;
#endif
	if ((lpDDSurfaceDesc->dwFlags & DDSD_PIXELFORMAT) == DDSD_PIXELFORMAT) {
		if ((lpDDSurfaceDesc->ddpfPixelFormat.dwFlags & DDPF_RGB) == DDPF_RGB) {
			switch (lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount) {
			case 8:
				format = SDL_PIXELFORMAT_INDEX8;
				break;
			case 16:
				format = SDL_PIXELFORMAT_RGB565;
				break;
			}
		}
	}

	if ((lpDDSurfaceDesc->dwFlags & (DDSD_WIDTH | DDSD_HEIGHT)) != (DDSD_WIDTH | DDSD_HEIGHT)) {
		return DDERR_INVALIDPARAMS;
	}

	int width = lpDDSurfaceDesc->dwWidth;
	int height = lpDDSurfaceDesc->dwHeight;
	if (width == 0 || height == 0) {
		return DDERR_INVALIDPARAMS;
	}
	*lplpDDSurface = static_cast<IDirectDrawSurface*>(new DirectDrawSurfaceImpl(width, height, format));
	return DD_OK;
}

HRESULT DirectDrawImpl::EnumDisplayModes(
	DWORD dwFlags,
	LPDDSURFACEDESC lpDDSurfaceDesc,
	LPVOID lpContext,
	LPDDENUMMODESCALLBACK lpEnumModesCallback
)
{
	SDL_DisplayID displayID = SDL_GetPrimaryDisplay();
	if (!displayID) {
		return DDERR_GENERIC;
	}

	int count_modes;
	SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(displayID, &count_modes);
	if (!modes) {
		return DDERR_GENERIC;
	}

	SDL_PixelFormat format;
	HRESULT status = S_OK;

	for (int i = 0; i < count_modes; i++) {
#ifdef MINIWIN_PIXELFORMAT
		format = MINIWIN_PIXELFORMAT;
#else
		format = modes[i]->format;
#endif

		const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(format);
		if (!details) {
			continue;
		}
		DDSURFACEDESC ddsd = {};
		ddsd.dwSize = sizeof(DDSURFACEDESC);
		ddsd.dwFlags = DDSD_WIDTH | DDSD_HEIGHT;
		ddsd.dwWidth = modes[i]->w;
		ddsd.dwHeight = modes[i]->h;
		ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
		ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB;
		ddsd.ddpfPixelFormat.dwRGBBitCount = details->bits_per_pixel;
		if (details->bits_per_pixel == 8) {
			ddsd.ddpfPixelFormat.dwFlags |= DDPF_PALETTEINDEXED8;
		}
		ddsd.ddpfPixelFormat.dwRBitMask = details->Rmask;
		ddsd.ddpfPixelFormat.dwGBitMask = details->Gmask;
		ddsd.ddpfPixelFormat.dwBBitMask = details->Bmask;
		ddsd.ddpfPixelFormat.dwRGBAlphaBitMask = details->Amask;

		if (!lpEnumModesCallback(&ddsd, lpContext)) {
			status = DDERR_GENERIC;
		}
	}
	SDL_free(modes);

	return status;
}

HRESULT DirectDrawImpl::FlipToGDISurface()
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT DirectDrawImpl::GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps)
{
	if (lpDDDriverCaps) {
		if (lpDDDriverCaps->dwSize >= sizeof(DDCAPS)) {
			lpDDDriverCaps->dwCaps2 = DDCAPS2_CERTIFIED; // Required to enable lighting
		}
	}

	if (lpDDHELCaps) {
		if (lpDDDriverCaps->dwSize >= sizeof(DDCAPS)) {
			lpDDDriverCaps->dwCaps2 = DDCAPS2_CERTIFIED; // Required to enable lighting
		}
	}

	return S_OK;
}

void EnumDevice(LPD3DENUMDEVICESCALLBACK cb, void* ctx, Direct3DRMRenderer* device, GUID deviceGuid)
{
	D3DDEVICEDESC halDesc = {};
	D3DDEVICEDESC helDesc = {};
	device->GetDesc(&halDesc, &helDesc);
	char* deviceNameDup = SDL_strdup(device->GetName());
	char* deviceDescDup = SDL_strdup("Miniwin driver");
	cb(&deviceGuid, deviceNameDup, deviceDescDup, &halDesc, &helDesc, ctx);
	SDL_free(deviceDescDup);
	SDL_free(deviceNameDup);
}

HRESULT DirectDrawImpl::EnumDevices(LPD3DENUMDEVICESCALLBACK cb, void* ctx)
{
	Direct3DRMSDL3GPU_EnumDevice(cb, ctx);
#ifdef USE_OPENGLES2
	OpenGLES2Renderer_EnumDevice(cb, ctx);
#endif
#ifdef USE_OPENGL1
	OpenGL1Renderer_EnumDevice(cb, ctx);
#endif
#ifdef _WIN32
	DirectX9Renderer_EnumDevice(cb, ctx);
#endif
	Direct3DRMSoftware_EnumDevice(cb, ctx);

	return S_OK;
}

HRESULT DirectDrawImpl::GetDisplayMode(LPDDSURFACEDESC lpDDSurfaceDesc)
{
	SDL_DisplayID displayID = SDL_GetPrimaryDisplay();
	if (!displayID) {
		return DDERR_GENERIC;
	}

	const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(displayID);
	if (!mode) {
		return DDERR_GENERIC;
	}

	SDL_PixelFormat format;
#ifdef MINIWIN_PIXELFORMAT
	format = MINIWIN_PIXELFORMAT;
#else
	format = mode->format;
#endif

	const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(format);

	lpDDSurfaceDesc->dwFlags = DDSD_WIDTH | DDSD_HEIGHT;
	lpDDSurfaceDesc->dwWidth = mode->w;
	lpDDSurfaceDesc->dwHeight = mode->h;
	lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount = details->bits_per_pixel;
	lpDDSurfaceDesc->ddpfPixelFormat.dwFlags = DDPF_RGB;
	if (details->bits_per_pixel == 8) {
		lpDDSurfaceDesc->ddpfPixelFormat.dwFlags |= DDPF_PALETTEINDEXED8;
	}
	lpDDSurfaceDesc->ddpfPixelFormat.dwRBitMask = details->Rmask;
	lpDDSurfaceDesc->ddpfPixelFormat.dwGBitMask = details->Gmask;
	lpDDSurfaceDesc->ddpfPixelFormat.dwBBitMask = details->Bmask;
	lpDDSurfaceDesc->ddpfPixelFormat.dwRGBAlphaBitMask = details->Amask;

	return DD_OK;
}

HRESULT DirectDrawImpl::RestoreDisplayMode()
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT DirectDrawImpl::SetCooperativeLevel(HWND hWnd, DDSCLFlags dwFlags)
{
	SDL_Window* sdlWindow = reinterpret_cast<SDL_Window*>(hWnd);

	if (m_virtualWidth == 0 || m_virtualHeight == 0) {
		if (!SDL_GetWindowSize(sdlWindow, &m_virtualWidth, &m_virtualHeight)) {
			SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_GetWindowSizeInPixels: %s", SDL_GetError());
		}
	}

	if (sdlWindow) {
		bool fullscreen;
		if ((dwFlags & DDSCL_NORMAL) == DDSCL_NORMAL) {
			fullscreen = false;
		}
		else if ((dwFlags & DDSCL_FULLSCREEN) == DDSCL_FULLSCREEN) {
			fullscreen = true;
		}
		else {
			return DDERR_INVALIDPARAMS;
		}

		if (!SDL_SetWindowFullscreen(sdlWindow, fullscreen)) {
#ifndef __EMSCRIPTEN__
			return DDERR_GENERIC;
#endif
		}
		DDWindow = sdlWindow;
	}
	return DD_OK;
}

HRESULT DirectDrawImpl::SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP)
{
	m_virtualWidth = dwWidth;
	m_virtualHeight = dwHeight;
	return DD_OK;
}

// IDirect3D2 interface
HRESULT DirectDrawImpl::CreateDevice(
	const GUID& guid,
	IDirectDrawSurface* pBackBuffer,
	IDirect3DDevice2** ppDirect3DDevice
)
{
	DDSURFACEDESC DDSDesc;
	DDSDesc.dwSize = sizeof(DDSURFACEDESC);
	pBackBuffer->GetSurfaceDesc(&DDSDesc);

	if (SDL_memcmp(&guid, &SDL3_GPU_GUID, sizeof(GUID)) == 0) {
		DDRenderer = Direct3DRMSDL3GPURenderer::Create(DDSDesc.dwWidth, DDSDesc.dwHeight);
	}
#ifdef USE_OPENGLES2
	else if (SDL_memcmp(&guid, &OpenGLES2_GUID, sizeof(GUID)) == 0) {
		DDRenderer = OpenGLES2Renderer::Create(DDSDesc.dwWidth, DDSDesc.dwHeight);
	}
#endif
#ifdef USE_OPENGL1
	else if (SDL_memcmp(&guid, &OpenGL1_GUID, sizeof(GUID)) == 0) {
		DDRenderer = OpenGL1Renderer::Create(DDSDesc.dwWidth, DDSDesc.dwHeight);
	}
#endif
#ifdef _WIN32
	else if (SDL_memcmp(&guid, &DirectX9_GUID, sizeof(GUID)) == 0) {
		DDRenderer = DirectX9Renderer::Create(DDSDesc.dwWidth, DDSDesc.dwHeight);
	}
#endif
	else if (SDL_memcmp(&guid, &SOFTWARE_GUID, sizeof(GUID)) == 0) {
		DDRenderer = new Direct3DRMSoftwareRenderer(DDSDesc.dwWidth, DDSDesc.dwHeight);
	}
	else {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Device GUID not recognized");
		return E_NOINTERFACE;
	}
	*ppDirect3DDevice = static_cast<IDirect3DDevice2*>(DDRenderer);
	return DD_OK;
}

HRESULT DirectDrawEnumerate(LPDDENUMCALLBACKA cb, void* context)
{
	const char* driverName = SDL_GetCurrentVideoDriver();
	char* driverNameDup = SDL_strdup(driverName);
	BOOL callback_result = cb(NULL, driverNameDup, NULL, context);
	SDL_free(driverNameDup);
	if (!callback_result) {
		return DDERR_GENERIC;
	}

	return DD_OK;
}

HRESULT DirectDrawCreate(LPGUID lpGuid, LPDIRECTDRAW* lplpDD, IUnknown* pUnkOuter)
{
	*lplpDD = new DirectDrawImpl;
	return DD_OK;
}
