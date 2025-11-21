#include "mortar/mortar_rect.h"
#include "sdl3_internal.h"

#include <stdlib.h>

SDL_Rect* rect_mortar_to_sdl3(const MORTAR_Rect* rect, SDL_Rect* storage)
{
	if (!storage) {
		abort();
	}
	if (!rect) {
		return nullptr;
	}
	storage->x = rect->x;
	storage->y = rect->y;
	storage->w = rect->w;
	storage->h = rect->h;
	return storage;
}

SDL_FRect* frect_mortar_to_sdl3(const MORTAR_FRect* rect, SDL_FRect* storage)
{
	if (!storage) {
		abort();
	}
	if (!rect) {
		return nullptr;
	}
	storage->x = rect->x;
	storage->y = rect->y;
	storage->w = rect->w;
	storage->h = rect->h;
	return storage;
}
