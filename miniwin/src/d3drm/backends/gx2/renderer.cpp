#include <coreinit/systeminfo.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <gfd.h>
#include <gx2/draw.h>
#include <gx2/mem.h>
#include <whb/gfx.h>
#include <whb/log_udp.h>
#include <whb/proc.h>
#include <whb/sdcard.h>

struct IsleGX2Backend {
    GX2RBuffer positionBuffer;
    GX2RBuffer colourBuffer;
    WHBGfxShaderGroup shaderGroup;
    bool rendering = false;

    void Init(const char* shaderPath) {
        WHBLogUdpInit();
        WHBProcInit();
        WHBGfxInit();
        WHBMountSdCard();
        char path[256];
        sprintf(path, "%s/%s", WHBGetSdCardMountPath(), shaderPath);
        FILE* f = fopen(path, "rb");
        if (!f) return;
        fseek(f, 0, SEEK_END);
        size_t fsize = ftell(f);
        rewind(f);
        char* data = (char*) malloc(fsize);
        fread(data, 1, fsize, f);
        WHBGfxLoadGFDShaderGroup(&shaderGroup, 0, data);
        free(data);
        WHBGfxInitShaderAttribute(&shaderGroup, "aPosition", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
        WHBGfxInitShaderAttribute(&shaderGroup, "aColour", 1, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
        WHBGfxInitFetchShader(&shaderGroup);
    }

    void StartFrame() {
        if (rendering) return;
        WHBGfxBeginRender();
        WHBGfxBeginRenderTV();
        rendering = true;
    }

    void EndFrame() {
        if (!rendering) return;
        WHBGfxFinishRenderTV();
        WHBGfxBeginRenderDRC();
        WHBGfxFinishRenderDRC();
        WHBGfxFinishRender();
        rendering = false;
    }

    void Clear(float r, float g, float b) {
        StartFrame();
        WHBGfxClearColor(r, g, b, 1.0f);
    }

    void Flip() {
        EndFrame();
    }

    void Shutdown() {
        WHBUnmountSdCard();
        WHBGfxShutdown();
        WHBProcShutdown();
        WHBLogUdpDeinit();
    }
};

static IsleGX2Backend g_backend;

extern "C" void IsleBackendInit(const char* shaderPath) { g_backend.Init(shaderPath); }
extern "C" void IsleBackendStartFrame() { g_backend.StartFrame(); }
extern "C" void IsleBackendEndFrame() { g_backend.EndFrame(); }
extern "C" void IsleBackendClear(float r, float g, float b) { g_backend.Clear(r, g, b); }
extern "C" void IsleBackendFlip() { g_backend.Flip(); }
extern "C" void IsleBackendShutdown() { g_backend.Shutdown(); }
