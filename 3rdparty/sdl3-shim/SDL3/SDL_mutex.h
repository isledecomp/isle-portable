#pragma once

#include <SDL2/SDL_mutex.h>

// https://wiki.libsdl.org/SDL3/README-migration#sdl_mutexh

#define SDL_Mutex SDL_mutex

#define SDL_Semaphore SDL_sem

#define SDL_WaitSemaphore SDL_SemWait
#define SDL_SignalSemaphore SDL_SemPost
