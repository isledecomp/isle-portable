#include "mortar/mortar.h"
#include "sdl3_internal.h"

const char* MORTAR_GetBackendDescription()
{
	static char buffer[256];
	SDL_snprintf(
		buffer,
		sizeof(buffer),
		"SDL3 version %d.%d.%d (%s)",
		SDL_VERSIONNUM_MAJOR(SDL3_versionnum),
		SDL_VERSIONNUM_MINOR(SDL3_versionnum),
		SDL_VERSIONNUM_MICRO(SDL3_versionnum),
		SDL_GetRevision()
	);
	return buffer;
}
