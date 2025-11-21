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
#ifdef USE_DIRECTX9
#include "d3drmrenderer_directx9.h"
#endif
#ifdef USE_SDL_GPU
#include "d3drmrenderer_sdl3gpu.h"
#endif
#ifdef USE_SOFTWARE_RENDER
#include "d3drmrenderer_software.h"
#endif
#ifdef USE_GXM
#include "d3drmrenderer_gxm.h"
#endif

Direct3DRMRenderer* CreateDirect3DRMRenderer(
	const IDirect3DMiniwin* d3d,
	const DDSURFACEDESC& DDSDesc,
	const GUID* guid
)
{
#ifdef USE_SDL_GPU
	if (MORTAR_memcmp(guid, &SDL3_GPU_GUID, sizeof(GUID)) == 0) {
		return Create_Direct3DRMSDL3GPURenderer(DDSDesc.dwWidth, DDSDesc.dwHeight);
	}
#endif
#ifdef USE_SOFTWARE_RENDER
	if (MORTAR_memcmp(guid, &SOFTWARE_GUID, sizeof(GUID)) == 0) {
		return new Direct3DRMSoftwareRenderer(DDSDesc.dwWidth, DDSDesc.dwHeight);
	}
#endif
#ifdef USE_OPENGLES3
	if (MORTAR_memcmp(guid, &OpenGLES3_GUID, sizeof(GUID)) == 0) {
		return OpenGLES3Renderer::Create(
			DDSDesc.dwWidth,
			DDSDesc.dwHeight,
			d3d->GetMSAASamples(),
			d3d->GetAnisotropic()
		);
	}
#endif
#ifdef USE_OPENGLES2
	if (MORTAR_memcmp(guid, &OpenGLES2_GUID, sizeof(GUID)) == 0) {
		return OpenGLES2Renderer::Create(DDSDesc.dwWidth, DDSDesc.dwHeight, d3d->GetAnisotropic());
	}
#endif
#ifdef USE_OPENGL1
	if (MORTAR_memcmp(guid, &OpenGL1_GUID, sizeof(GUID)) == 0) {
		return OpenGL1Renderer::Create(DDSDesc.dwWidth, DDSDesc.dwHeight, d3d->GetMSAASamples());
	}
#endif
#ifdef USE_CITRO3D
	if (MORTAR_memcmp(guid, &Citro3D_GUID, sizeof(GUID)) == 0) {
		return new Citro3DRenderer(DDSDesc.dwWidth, DDSDesc.dwHeight);
	}
#endif
#ifdef USE_DIRECTX9
	if (MORTAR_memcmp(guid, &DirectX9_GUID, sizeof(GUID)) == 0) {
		return DirectX9Renderer::Create(DDSDesc.dwWidth, DDSDesc.dwHeight);
	}
#endif
#ifdef USE_GXM
	if (MORTAR_memcmp(guid, &GXM_GUID, sizeof(GUID)) == 0) {
		return GXMRenderer::Create(DDSDesc.dwWidth, DDSDesc.dwHeight, d3d->GetMSAASamples());
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
#ifdef USE_DIRECTX9
	DirectX9Renderer_EnumDevice(cb, ctx);
#endif
#ifdef USE_SOFTWARE_RENDER
	Direct3DRMSoftware_EnumDevice(cb, ctx);
#endif
#ifdef USE_GXM
	GXMRenderer_EnumDevice(cb, ctx);
#endif
}
