#include "miniwin_config.h"
#include "miniwin_d3drm.h"
#include "miniwin_d3drm_sdl3gpu.h"
#include "miniwin_ddraw_sdl3gpu.h"
#include "miniwin_p.h"

#define RGBA_MAKE(r, g, b, a) ((D3DCOLOR) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b)))

namespace
{
MiniwinBackendType g_backendType = MiniwinBackendType::eInvalid;
}

void Miniwin_ConfigureBackend(MiniwinBackendType type)
{
	g_backendType = type;
}

MiniwinBackendType Miniwin_StringToBackendType(const char* str)
{
	if (SDL_strcasecmp(str, "sdl3gpu") == 0) {
		return MiniwinBackendType::eSDL3GPU;
	}
	return MiniwinBackendType::eInvalid;
}

std::string Miniwin_BackendTypeToString(MiniwinBackendType type)
{
	switch (type) {
	case MiniwinBackendType::eSDL3GPU:
		return "sdl3gpu";
	default:
		return "<invalid>";
	}
}

HRESULT WINAPI Direct3DRMCreate(IDirect3DRM** direct3DRM)
{
	switch (g_backendType) {
	case MiniwinBackendType::eSDL3GPU:
		*direct3DRM = new Direct3DRM_SDL3GPUImpl;
		return DD_OK;
	default:
		*direct3DRM = nullptr;
		return DDERR_GENERIC;
	}
}

D3DCOLOR D3DRMCreateColorRGBA(D3DVALUE red, D3DVALUE green, D3DVALUE blue, D3DVALUE alpha)
{
	return RGBA_MAKE((int) (255.f * red), (int) (255.f * green), (int) (255.f * blue), (int) (255.f * alpha));
}

HRESULT DirectDrawCreate(LPGUID lpGuid, LPDIRECTDRAW* lplpDD, IUnknown* pUnkOuter)
{
	if (lpGuid) {
		MINIWIN_NOT_IMPLEMENTED();
	}
	switch (g_backendType) {
	case MiniwinBackendType::eSDL3GPU:
		return DirectDrawCreate_SDL3GPU(lpGuid, lplpDD, pUnkOuter);
	default:
		*lplpDD = nullptr;
		return DDERR_GENERIC;
	}
}

HRESULT DirectDrawEnumerate(LPDDENUMCALLBACKA cb, void* context)
{
	switch (g_backendType) {
	case MiniwinBackendType::eSDL3GPU:
		return DirectDrawEnumerate_SDL3GPU(cb, context);
	default:
		return DDERR_GENERIC;
	}
}
