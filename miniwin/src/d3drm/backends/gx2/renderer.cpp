#include "d3drmrenderer_gx2.h"
#include <coreinit/systeminfo.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <gfd.h>
#include <gx2/draw.h>
#include <gx2/mem.h>
#include <gx2/clear.h>
#include <gx2/state.h>
#include <whb/gfx.h>
#include <whb/log_udp.h>
#include <whb/proc.h>
#include <whb/sdcard.h>

#ifndef D3D_OK
#define D3D_OK S_OK
#endif

static struct GX2Backend {
    bool initialized = false;
    bool rendering = false;

    void Init()
    {
        if (initialized)
            return;

        WHBLogUdpInit();
        WHBProcInit();
        WHBGfxInit();
        WHBMountSdCard();

        initialized = true;
    }

    void StartFrame()
    {
        if (rendering)
            return;

        WHBGfxBeginRender();
        WHBGfxBeginRenderTV();
        WHBGfxBeginRenderDRC();
        rendering = true;
    }

    void EndFrame()
    {
        if (!rendering)
            return;

        WHBGfxFinishRenderDRC();
        WHBGfxFinishRenderTV();
        WHBGfxFinishRender();
        rendering = false;
    }

    void Clear(float r, float g, float b)
    {
        StartFrame();

        GX2ColorBuffer* tvBuffer = WHBGfxGetTVColourBuffer();
        GX2ColorBuffer* drcBuffer = WHBGfxGetDRCColourBuffer();

        GX2ClearColor(tvBuffer, r, g, b, 1.0f);
        GX2ClearColor(drcBuffer, r, g, b, 1.0f);
    }

    void Flip()
    {
        EndFrame();
    }

    void Shutdown()
    {
        if (!initialized)
            return;

        WHBUnmountSdCard();
        WHBGfxShutdown();
        WHBProcShutdown();
        WHBLogUdpDeinit();
        initialized = false;
    }
} g_backend;

GX2Renderer::GX2Renderer(DWORD width, DWORD height)
{
    m_width = width;
    m_height = height;
    m_virtualWidth = width;
    m_virtualHeight = height;

    g_backend.Init();
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

void GX2Renderer::EnableTransparency() {}

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
