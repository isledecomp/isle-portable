#include "miniwin_d3drmlight_sdl3gpu.h"
#include "miniwin_p.h"

Direct3DRMLight_SDL3GPUImpl::Direct3DRMLight_SDL3GPUImpl(float r, float g, float b)
{
	SetColorRGB(r, g, b);
}

HRESULT Direct3DRMLight_SDL3GPUImpl::SetColorRGB(float r, float g, float b)
{
	m_color = (0xFF << 24) | (static_cast<BYTE>(r * 255.0f) << 16) | (static_cast<BYTE>(g * 255.0f) << 8) |
			  (static_cast<BYTE>(b * 255.0f));
	return DD_OK;
}
