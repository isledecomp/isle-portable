#include "mortar/mortar_keyboard.h"
#include "sdl3_internal.h"

const bool* MORTAR_GetKeyboardState(int* numkeys)
{
	return SDL_GetKeyboardState(numkeys);
}
