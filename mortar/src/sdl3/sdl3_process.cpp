#include "mortar/mortar_process.h"
#include "sdl3_internal.h"

MORTAR_Process* MORTAR_CreateProcess(const char* const* args, bool pipe_stdio)
{
	return (MORTAR_Process*) SDL_CreateProcess(args, pipe_stdio);
}
