#pragma once

#include <SDL2/SDL_timer.h>

// https://wiki.libsdl.org/SDL3/README-migration#sdl_timerh
// https://wiki.libsdl.org/SDL3/README-migration#sdl_timerh | SDL_GetTicksNS()

#define SDL_GetTicksNS SDL_GetTicks

// time is in miliseconds not nanoseconds
#define SDL_NS_TO_MS(MS) (MS)
