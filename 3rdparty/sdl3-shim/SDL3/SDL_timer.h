#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_mutex.h>

// https://wiki.libsdl.org/SDL3/README-migration#sdl_timerh
// https://wiki.libsdl.org/SDL3/README-migration#sdl_timerh | SDL_GetTicksNS()

#define SDL_GetTicksNS SDL_GetTicks

// time is in miliseconds not nanoseconds
#define SDL_NS_TO_MS(MS) (MS)


typedef Uint32 SDL3_TimerCallback (void *userdata, SDL_TimerID timerID, Uint32 interval);

struct SDL2TimerShimData {
	SDL3_TimerCallback *callback2;
	void *userdata2;
	SDL_TimerID id;
};

static std::map<SDL_TimerID, void*> g_timers;
static SDL_mutex *g_timerMutex = NULL;

inline Uint32 shim_timer_callback (Uint32 interval, void *param)
{
	const auto shim = static_cast<SDL2TimerShimData*>(param);
	const Uint32 next = shim->callback2(shim->userdata2, shim->id, interval);

	if (next == 0) {
		SDL_LockMutex(g_timerMutex);
		g_timers.erase(shim->id);
		SDL_free(shim);
		SDL_UnlockMutex(g_timerMutex);
	}

	return next;
}

inline SDL_TimerID SDL_AddTimer(Uint32 interval, SDL3_TimerCallback callback3, void* userdata)
{
	const auto shim = static_cast<SDL2TimerShimData*>(SDL_malloc(sizeof(SDL2TimerShimData)));
	shim->callback2 = callback3;
	shim->userdata2 = userdata;

	const SDL_TimerID id = ::SDL_AddTimer(interval, shim_timer_callback, shim);
	shim->id = id;

	SDL_LockMutex(g_timerMutex);
	g_timers[id] = shim;
	SDL_UnlockMutex(g_timerMutex);

	return id;
}

inline SDL_bool SDL_RemoveTimer2(SDL_TimerID id)
{
	SDL_LockMutex(g_timerMutex);
	if (const auto it = g_timers.find(id); it != g_timers.end()) {
		SDL_free(it->second);
		g_timers.erase(it);
	}
	SDL_UnlockMutex(g_timerMutex);

	return ::SDL_RemoveTimer(id);
}

#define SDL_RemoveTimer SDL_RemoveTimer2
