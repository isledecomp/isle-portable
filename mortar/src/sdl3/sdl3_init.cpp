#include "mortar/mortar_init.h"
#include "sdl3_internal.h"

int SDL3_versionnum;
SDL3_Symbols SDL3;

bool MORTAR_Init()
{
	if (!load_sdl3_api()) {
		return false;
	}
	SDL3_versionnum = SDL_GetVersion();

	SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC)) {
		return false;
	}
	return true;
}

void MORTAR_Quit()
{
	SDL_Quit();
	unload_sdl3_api();
}
