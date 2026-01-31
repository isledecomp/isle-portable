#include "mortar/mortar_cpuinfo.h"
#include "sdl3_internal.h"

bool MORTAR_HasMMX()
{
	return SDL_HasMMX();
}

bool MORTAR_HasSSE2()
{
	return SDL_HasSSE2();
}

bool MORTAR_HasNEON()
{
	return SDL_HasNEON();
}

int MORTAR_GetSystemRAM()
{
	return SDL_GetSystemRAM();
}
