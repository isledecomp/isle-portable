#include "mortar/mortar_timer.h"
#include "sdl3_internal.h"

void MORTAR_Delay(uint32_t ms)
{
	SDL_Delay(ms);
}

uint64_t MORTAR_GetTicks()
{
	return SDL_GetTicks();
}

uint64_t MORTAR_GetTicksNS()
{
	return SDL_GetTicksNS();
}

uint64_t MORTAR_GetPerformanceFrequency()
{
	return SDL_GetPerformanceFrequency();
}

uint64_t MORTAR_GetPerformanceCounter()
{
	return SDL_GetPerformanceCounter();
}

MORTAR_TimerID MORTAR_AddTimer(uint32_t interval, MORTAR_TimerCallback callback, void* userdata)
{
	return (SDL_TimerID) SDL_AddTimer(interval, callback, userdata);
}

bool MORTAR_RemoveTimer(MORTAR_TimerID id)
{
	return SDL_RemoveTimer(id);
}
