#define SDL_MAIN_HANDLED
#define IMPLEMENT_DYN_SDL3
#include "mortar/mortar_main.h"
#include "sdl3_internal.h"

#include <stdlib.h>

static MORTAR_AppInit_cbfn* g_mortar_AppInit;
static MORTAR_AppIterate_cbfn* g_mortar_AppIterate;
static MORTAR_AppEvent_cbfn* g_mortar_AppEvent;
static MORTAR_AppQuit_cbfn* g_mortar_AppQuit;

int SDL3_versionnum;

static SDL_AppResult appresult_mortar_to_sdl3(MORTAR_AppResult result)
{
	switch (result) {
	case MORTAR_APP_CONTINUE:
		return SDL_APP_CONTINUE;
	case MORTAR_APP_SUCCESS:
		return SDL_APP_SUCCESS;
	case MORTAR_APP_FAILURE:
		return SDL_APP_FAILURE;
	default:
		abort();
	}
}

static MORTAR_AppResult appresult_sdl3_to_mortar(SDL_AppResult result)
{
	switch (result) {
	case SDL_APP_CONTINUE:
		return MORTAR_APP_CONTINUE;
	case SDL_APP_SUCCESS:
		return MORTAR_APP_SUCCESS;
	case SDL_APP_FAILURE:
		return MORTAR_APP_FAILURE;
	default:
		abort();
	}
}

static SDL_AppResult SDLCALL sdl3_AppInit(void** appstate, int argc, char* argv[])
{
	return appresult_mortar_to_sdl3(g_mortar_AppInit(appstate, argc, argv));
}

static SDL_AppResult SDLCALL sdl3_AppIterate(void* appstate)
{
	return appresult_mortar_to_sdl3(g_mortar_AppIterate(appstate));
}

static SDL_AppResult SDLCALL sdl3_AppEvent(void* appstate, SDL_Event* sdl3_event)
{
	MORTAR_Event mortar_event;
	if (!event_sdl3_to_mortar(sdl3_event, &mortar_event)) {
		return SDL_APP_CONTINUE;
	}
	MORTAR_AppResult mortar_app_result = g_mortar_AppEvent(appstate, &mortar_event);
	return appresult_mortar_to_sdl3(mortar_app_result);
}

static void SDLCALL sdl3_AppQuit(void* appstate, SDL_AppResult result)
{
	g_mortar_AppQuit(appstate, appresult_sdl3_to_mortar(result));
}

static int mortar_sdl3_main(int argc, char* argv[])
{
	int result = SDL_EnterAppMainCallbacks(argc, argv, sdl3_AppInit, sdl3_AppIterate, sdl3_AppEvent, sdl3_AppQuit);
	return result;
}

bool MORTAR_SDL3_Initialize(void)
{
	return load_sdl3_api();
}

int MORTAR_SDL3_main(
	int argc,
	char* argv[],
	MORTAR_AppInit_cbfn* init,
	MORTAR_AppIterate_cbfn* iterate,
	MORTAR_AppEvent_cbfn* event,
	MORTAR_AppQuit_cbfn* quit
)
{
	SDL3_versionnum = SDL_GetVersion();
	g_mortar_AppInit = init;
	g_mortar_AppIterate = iterate;
	g_mortar_AppEvent = event;
	g_mortar_AppQuit = quit;
	int result = SDL_RunApp(argc, argv, mortar_sdl3_main, NULL);
	unload_sdl3_api();
	return result;
}
