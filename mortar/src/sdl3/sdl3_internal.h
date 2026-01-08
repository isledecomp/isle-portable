#pragma once

#include "mortar/backends/sdl3_dynamic.h"
#include "mortar/mortar.h"

extern int SDL3_versionnum;

typedef struct MORTAR_SDL_Palette {
	MORTAR_Palette mortar;
	SDL_Palette* sdl3_palette;
	int refcount;
} MORTAR_SDL_Palette;

extern void event_mortar_to_sdl3(const MORTAR_Event* mortar_event, SDL_Event* sdl3_event);
extern bool event_sdl3_to_mortar(const SDL_Event* sdl3_event, MORTAR_Event* mortar_event);

extern SDL_Rect* rect_mortar_to_sdl3(const MORTAR_Rect* rect, SDL_Rect* storage);
extern SDL_FRect* frect_mortar_to_sdl3(const MORTAR_FRect* rect, SDL_FRect* storage);

extern SDL_Keycode keycode_mortar_to_sdl3(MORTAR_Keycode keycode);
extern MORTAR_Keycode keycode_sdl3_to_mortar(SDL_Keycode keycode);
extern SDL_Keymod keymod_mortar_to_sdl3(MORTAR_Keymod keymod);
extern MORTAR_Keymod keymod_sdl3_to_mortar(SDL_Keymod keymod);

extern SDL_MouseButtonFlags mousebuttonflags_mortar_to_sdl3(MORTAR_MouseButtonFlags flags);
extern MORTAR_MouseButtonFlags mousebuttonflags_sdl3_to_mortar(SDL_MouseButtonFlags flags);

extern SDL_Window* window_mortar_to_sdl3(MORTAR_Window* mortar);

extern SDL_Surface* surface_mortar_to_sdl3(MORTAR_Surface* surface);

extern MORTAR_PixelFormat pixelformat_sdl3_to_mortar(SDL_PixelFormat format);
extern SDL_PixelFormat pixelformat_mortar_to_sdl3(MORTAR_PixelFormat format);

extern SDL_Color color_mortar_to_sdl3(const MORTAR_Color* color);

extern SDL_Joystick* joystick_mortar_to_sdl3(MORTAR_Joystick* joystick);

extern SDL_IOStream* iostream_mortar_to_sdl3(MORTAR_IOStream* stream);

extern SDL_Palette* palette_mortar_to_sdl3(const MORTAR_Palette* mortar_palette);
