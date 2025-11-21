#include "mortar/mortar_mutex.h"
#include "sdl3_internal.h"

SDL_Mutex* mutex_mortar_to_sdl3(MORTAR_Mutex* mutex)
{
	return (SDL_Mutex*) mutex;
}

SDL_Semaphore* semaphore_mortar_to_sdl3(MORTAR_Semaphore* sem)
{
	return (SDL_Semaphore*) sem;
}

MORTAR_Mutex* MORTAR_CreateMutex()
{
	return (MORTAR_Mutex*) SDL_CreateMutex();
}

void MORTAR_LockMutex(MORTAR_Mutex* mutex)
{
	SDL_LockMutex(mutex_mortar_to_sdl3(mutex));
}

void MORTAR_UnlockMutex(MORTAR_Mutex* mutex)
{
	SDL_UnlockMutex(mutex_mortar_to_sdl3(mutex));
}

void MORTAR_DestroyMutex(MORTAR_Mutex* mutex)
{
	SDL_DestroyMutex(mutex_mortar_to_sdl3(mutex));
}

MORTAR_Semaphore* MORTAR_CreateSemaphore(uint32_t initial_value)
{
	return (MORTAR_Semaphore*) SDL_CreateSemaphore(initial_value);
}

void MORTAR_SignalSemaphore(MORTAR_Semaphore* sem)
{
	SDL_SignalSemaphore(semaphore_mortar_to_sdl3(sem));
}

void MORTAR_WaitSemaphore(MORTAR_Semaphore* sem)
{
	SDL_WaitSemaphore(semaphore_mortar_to_sdl3(sem));
}

void MORTAR_DestroySemaphore(MORTAR_Semaphore* sem)
{
	SDL_DestroySemaphore(semaphore_mortar_to_sdl3(sem));
}
