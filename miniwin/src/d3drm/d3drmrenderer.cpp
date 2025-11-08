#include "d3drmrenderer.h"
#ifdef USE_OPENGL1
#include "d3drmrenderer_opengl1.h"
#endif
#ifdef USE_OPENGLES2
#include "d3drmrenderer_opengles2.h"
#endif
#ifdef USE_OPENGLES3
#include "d3drmrenderer_opengles3.h"
#endif
#ifdef USE_CITRO3D
#include "d3drmrenderer_citro3d.h"
#endif
#ifdef USE_GX2
#include "d3drmrenderer_gx2.h"
#endif
#ifdef USE_DIRECTX9
#include "d3drmrenderer_directx9.h"
#endif
#ifdef USE_SDL_GPU
#include "d3drmrenderer_sdl3gpu.h"
#endif
#ifdef USE_SOFTWARE_RENDER
#include "d3drmrenderer_software.h"
#endif

Direct3DRMRenderer* CreateDirect3DRMRenderer(
	const IDirect3DMiniwin* d3d,
	const DDSURFACEDESC& DDSDesc,
	const GUID* guid
)
{
#ifdef USE_SDL_GPU
	if (SDL_memcmp(guid, &SDL3_GPU_GUID, sizeof(GUID)) == 0) {
		return Direct3DRMSDL3GPURenderer::Create(DDSDesc.dwWidth, DDSDesc.dwHeight);
	}
#endif
#ifdef USE_SOFTWARE_RENDER
	if (SDL_memcmp(guid, &SOFTWARE_GUID, sizeof(GUID)) == 0) {
		return new Direct3DRMSoftwareRenderer(DDSDesc.dwWidth, DDSDesc.dwHeight);
	}
#endif
#ifdef USE_OPENGLES3
	if (SDL_memcmp(guid, &OpenGLES3_GUID, sizeof(GUID)) == 0) {
		return OpenGLES3Renderer::Create(
			DDSDesc.dwWidth,
			DDSDesc.dwHeight,
			d3d->GetMSAASamples(),
			d3d->GetAnisotropic()
		);
	}
#endif
#ifdef USE_OPENGLES2
	if (SDL_memcmp(guid, &OpenGLES2_GUID, sizeof(GUID)) == 0) {
		return OpenGLES2Renderer::Create(DDSDesc.dwWidth, DDSDesc.dwHeight, d3d->GetAnisotropic());
	}
#endif
#ifdef USE_OPENGL1
	if (SDL_memcmp(guid, &OpenGL1_GUID, sizeof(GUID)) == 0) {
		return OpenGL1Renderer::Create(DDSDesc.dwWidth, DDSDesc.dwHeight, d3d->GetMSAASamples());
	}
#endif
#ifdef USE_CITRO3D
	if (SDL_memcmp(guid, &Citro3D_GUID, sizeof(GUID)) == 0) {
		return new Citro3DRenderer(DDSDesc.dwWidth, DDSDesc.dwHeight);
	}
#endif
#ifdef USE_GX2
	if (SDL_memcmp(guid, &GX2_GUID, sizeof(GUID)) == 0) {
		return new GX2Renderer(DDSDesc.dwWidth, DDSDesc.dwHeight);
	}
#endif
#ifdef USE_DIRECTX9
	if (SDL_memcmp(guid, &DirectX9_GUID, sizeof(GUID)) == 0) {
		return DirectX9Renderer::Create(DDSDesc.dwWidth, DDSDesc.dwHeight);
	}
#endif
	return nullptr;
}

void Direct3DRMRenderer_EnumDevices(const IDirect3DMiniwin* d3d, LPD3DENUMDEVICESCALLBACK cb, void* ctx)
{
#ifdef USE_SDL_GPU
	Direct3DRMSDL3GPU_EnumDevice(cb, ctx);
#endif
#ifdef USE_OPENGLES3
	OpenGLES3Renderer_EnumDevice(d3d, cb, ctx);
#endif
#ifdef USE_OPENGLES2
	OpenGLES2Renderer_EnumDevice(d3d, cb, ctx);
#endif
#ifdef USE_OPENGL1
	OpenGL1Renderer_EnumDevice(d3d, cb, ctx);
#endif
#ifdef USE_CITRO3D
	Citro3DRenderer_EnumDevice(cb, ctx);
#endif
#ifdef USE_GX2
	GX2Renderer_EnumDevice(cb, ctx);
#endif
#ifdef USE_DIRECTX9
	DirectX9Renderer_EnumDevice(cb, ctx);
#endif
#ifdef USE_SOFTWARE_RENDER
	Direct3DRMSoftware_EnumDevice(cb, ctx);
#endif
}
