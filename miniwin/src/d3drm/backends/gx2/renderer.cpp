#include "d3drmrenderer_gx2.h"
#include <coreinit/systeminfo.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <gfd.h>
#include <gx2/draw.h>
#include <gx2/mem.h>
#include <whb/gfx.h>
#include <whb/log_udp.h>
#include <whb/proc.h>
#include <whb/sdcard.h>

#ifndef D3D_OK
#define D3D_OK S_OK
#endif

static struct IsleGX2Backend {
    bool initialized = false;
    bool rendering = false;

    void Init(const char* shaderPath)
    {
        if (initialized) return;
        WHBLogUdpInit();
        WHBProcInit();
        WHBGfxInit();
        WHBMountSdCard();
        initialized = true;
    }

    void StartFrame()
    {
        if (rendering) return;
        WHBGfxBeginRender();
        WHBGfxBeginRenderTV();
        rendering = true;
    }

    void EndFrame()
    {
        if (!rendering) return;
        WHBGfxFinishRenderTV();
        WHBGfxBeginRenderDRC();
        WHBGfxFinishRenderDRC();
        WHBGfxFinishRender();
        rendering = false;
    }

    void Clear(float r, float g, float b)
    {
        StartFrame();
        WHBGfxClearColor(r, g, b, 1.0f);
    }

    void Flip()
    {
        EndFrame();
    }

    void Shutdown()
    {
        if (!initialized) return;
        WHBUnmountSdCard();
        WHBGfxShutdown();
        WHBProcShutdown();
        WHBLogUdpDeinit();
        initialized = false;
    }

} g_backend;

// ------------------------------------
// GX2Renderer Implementation
// ------------------------------------

GX2Renderer::GX2Renderer(DWORD width, DWORD height)
{
    m_width = width;
    m_height = height;
    m_virtualWidth = width;
    m_virtualHeight = height;

    g_backend.Init("content/renderer.gsh");
}

GX2Renderer::~GX2Renderer()
{
    g_backend.Shutdown();
}

HRESULT GX2Renderer::BeginFrame()
{
    g_backend.StartFrame();
    return D3D_OK;
}

void GX2Renderer::EnableTransparency()
{
    // GX2 blending can be configured here if needed
}

HRESULT GX2Renderer::FinalizeFrame()
{
    g_backend.EndFrame();
    return D3D_OK;
}

void GX2Renderer::Clear(float r, float g, float b)
{
    g_backend.Clear(r, g, b);
}

void GX2Renderer::Flip()
{
    g_backend.Flip();
}

void GX2Renderer::Resize(int width, int height, const ViewportTransform& viewportTransform)
{
    m_width = width;
    m_height = height;
    m_viewportTransform = viewportTransform;
}

void GX2Renderer::PushLights(const SceneLight* vertices, size_t count) {}
void GX2Renderer::SetProjection(const D3DRMMATRIX4D&, D3DVALUE, D3DVALUE) {}
void GX2Renderer::SetFrustumPlanes(const Plane*) {}
Uint32 GX2Renderer::GetTextureId(IDirect3DRMTexture*, bool, float, float) { return 0; }
Uint32 GX2Renderer::GetMeshId(IDirect3DRMMesh*, const MeshGroup*) { return 0; }
void GX2Renderer::SubmitDraw(DWORD, const D3DRMMATRIX4D&, const D3DRMMATRIX4D&, const D3DRMMATRIX4D&, const Matrix3x3&, const Appearance&) {}
void GX2Renderer::Draw2DImage(Uint32, const SDL_Rect&, const SDL_Rect&, FColor) {}
void GX2Renderer::Download(SDL_Surface*) {}
void GX2Renderer::SetDither(bool) {}
