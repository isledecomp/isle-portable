#include "mortar/mortar_mouse.h"
#include "sdl3_internal.h"

#include <stdlib.h>

static SDL_SystemCursor systemcursor_mortar_to_sdl3(MORTAR_SystemCursor id)
{
	switch (id) {
	case MORTAR_SYSTEM_CURSOR_DEFAULT:
		return SDL_SYSTEM_CURSOR_DEFAULT;
	case MORTAR_SYSTEM_CURSOR_WAIT:
		return SDL_SYSTEM_CURSOR_WAIT;
	case MORTAR_SYSTEM_CURSOR_NOT_ALLOWED:
		return SDL_SYSTEM_CURSOR_NOT_ALLOWED;
	default:
		abort();
	}
}

SDL_MouseButtonFlags mousebuttonflags_mortar_to_sdl3(MORTAR_MouseButtonFlags flags)
{
	SDL_MouseButtonFlags result = 0;
	if (flags & MORTAR_BUTTON_LMASK) {
		result |= SDL_BUTTON_LMASK;
	}
	if (flags & MORTAR_BUTTON_MMASK) {
		result |= SDL_BUTTON_MMASK;
	}
	if (flags & MORTAR_BUTTON_RMASK) {
		result |= SDL_BUTTON_RMASK;
	}
	if (flags & MORTAR_BUTTON_X1MASK) {
		result |= SDL_BUTTON_X1MASK;
	}
	if (flags & MORTAR_BUTTON_X2MASK) {
		result |= SDL_BUTTON_X2MASK;
	}
	return result;
}

MORTAR_MouseButtonFlags mousebuttonflags_sdl3_to_mortar(SDL_MouseButtonFlags flags)
{
	MORTAR_MouseButtonFlags result = (MORTAR_MouseButtonFlags) 0;
	if (flags & SDL_BUTTON_LMASK) {
		result |= MORTAR_BUTTON_LMASK;
	}
	if (flags & SDL_BUTTON_MMASK) {
		result |= MORTAR_BUTTON_MMASK;
	}
	if (flags & SDL_BUTTON_RMASK) {
		result |= MORTAR_BUTTON_RMASK;
	}
	if (flags & SDL_BUTTON_X1MASK) {
		result |= MORTAR_BUTTON_X1MASK;
	}
	if (flags & SDL_BUTTON_X2MASK) {
		result |= MORTAR_BUTTON_X2MASK;
	}
	return result;
}

MORTAR_MouseButtonFlags mouse_button_flags_sdl3_to_mortar(SDL_MouseButtonFlags flags)
{
	MORTAR_MouseButtonFlags result = (MORTAR_MouseButtonFlags) 0;
	if (flags & SDL_BUTTON_LMASK) {
		result |= MORTAR_BUTTON_LMASK;
	}
	if (flags & SDL_BUTTON_MMASK) {
		result |= MORTAR_BUTTON_MMASK;
	}
	if (flags & SDL_BUTTON_RMASK) {
		result |= MORTAR_BUTTON_RMASK;
	}
	if (flags & SDL_BUTTON_X1MASK) {
		result |= MORTAR_BUTTON_X1MASK;
	}
	if (flags & SDL_BUTTON_X2MASK) {
		result |= MORTAR_BUTTON_X2MASK;
	}
	return result;
}

bool MORTAR_ShowCursor()
{
	return SDL_ShowCursor();
}

bool MORTAR_HideCursor()
{
	return SDL_HideCursor();
}

void MORTAR_WarpMouseInWindow(MORTAR_Window* window, float x, float y)
{
	SDL_WarpMouseInWindow(window_mortar_to_sdl3(window), x, y);
}

bool MORTAR_SetCursor(MORTAR_Cursor* cursor)
{
	return SDL_SetCursor((SDL_Cursor*) cursor);
}

MORTAR_Cursor* MORTAR_CreateSystemCursor(MORTAR_SystemCursor id)
{
	return (MORTAR_Cursor*) SDL_CreateSystemCursor(systemcursor_mortar_to_sdl3(id));
}

MORTAR_MouseButtonFlags MORTAR_GetMouseState(float* x, float* y)
{
	return mouse_button_flags_sdl3_to_mortar(SDL_GetMouseState(x, y));
}
