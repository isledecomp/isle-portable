#include "mortar/mortar_system.h"
#include "sdl3_internal.h"

#include <SDL3/SDL_system.h>

const char* MORTAR_GetAndroidExternalStoragePath()
{
#ifdef SDL_PLAFORM_ANDROID
	return SDL_GetAndroidExternalStoragePath();
#else
	return NULL;
#endif
}
