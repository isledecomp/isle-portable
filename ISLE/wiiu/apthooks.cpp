#include "apthooks.h"

#include "legomain.h"
#include "misc.h"

#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <gx2/draw.h>
#include <gx2/mem.h>
#include <gx2/state.h>
#include <proc_ui/procui.h>
#include <whb/gfx.h>
#include <SDL3/SDL_log.h>

static bool running = true;
static bool inBackground = false;

#ifdef USE_GX2
static void ReleaseGraphics()
{
    SDL_Log("[APTHooks] Releasing GX2 resources...\n");
    WHBGfxFinishRender();
    GX2Flush();
    GX2Shutdown();
}

static void RestoreGraphics()
{
    SDL_Log("[APTHooks] Restoring GX2 resources...\n");
    WHBGfxInit();
}
#endif

void WIIU_ProcessCallbacks()
{
    ProcUIStatus status = ProcUIProcessMessages(true);

    switch (status)
    {
        case PROCUI_STATUS_IN_FOREGROUND:
            if (inBackground) {
                inBackground = false;
                RestoreGraphics();
                Lego()->Resume();
                SDL_Log("[APTHooks] App resumed\n");
            }
            break;

        case PROCUI_STATUS_IN_BACKGROUND:
            if (!inBackground) {
                inBackground = true;
                Lego()->Pause();
                ReleaseGraphics();
                SDL_Log("[APTHooks] App backgrounded\n");
            }
            break;

        case PROCUI_STATUS_RELEASE_FOREGROUND:
            Lego()->Pause();
            ReleaseGraphics();
            SDL_Log("[APTHooks] Yielding to system\n");
            ProcUIDrawDoneRelease();
            break;

        case PROCUI_STATUS_EXITING:
            Lego()->CloseMainWindow();
            running = false;
            SDL_Log("[APTHooks] App exiting\n");
            break;

        default:
            break;
    }
}

bool WIIU_AppIsRunning()
{
    return running;
}

void WIIU_SetupAptHooks()
{
    SDL_Log("[APTHooks] Initializing lifecycle hooks\n");
    running = true;
    inBackground = false;
    ProcUIInit(nullptr);
}
