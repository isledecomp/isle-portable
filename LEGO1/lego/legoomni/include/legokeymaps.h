#ifndef LEGOKEYMAPS_H
#define LEGOKEYMAPS_H

#include "decomp.h"
#include "lego1_export.h"

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_scancode.h>
#ifdef MINIWIN
#include "miniwin/windows.h"
#else
#include <windows.h>
#endif

#include <map>
#include <variant>

struct KeyMaps {
	SDL_Scancode k_forward[2];
	SDL_Scancode k_back[2];
	SDL_Scancode k_left[2];
	SDL_Scancode k_right[2];
	SDL_Scancode k_sprint[2];
};

extern KeyMaps g_keyMaps;

#endif
