#include "mortar/mortar_error.h"
#include "sdl3_internal.h"

const char* MORTAR_GetError()
{
	return SDL_GetError();
}
