#include "mortar/mortar_thread.h"
#include "sdl3_internal.h"

void MORTAR_WaitThread(MORTAR_Thread* thread, int* status)
{
	SDL_WaitThread((SDL_Thread*) thread, status);
}

MORTAR_Thread* MORTAR_CreateThread(MORTAR_ThreadFunction fn, void* data, uint32_t stacksize)
{
	SDL_PropertiesID props = SDL_CreateProperties();
	if (!props) {
		return nullptr;
	}
	SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_ENTRY_FUNCTION_POINTER, (void*) fn);
	SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_USERDATA_POINTER, data);
	SDL_SetNumberProperty(props, SDL_PROP_THREAD_CREATE_STACKSIZE_NUMBER, stacksize);
	SDL_Thread* thread = SDL_CreateThreadWithProperties(props);
	SDL_DestroyProperties(props);
	return (MORTAR_Thread*) thread;
}
