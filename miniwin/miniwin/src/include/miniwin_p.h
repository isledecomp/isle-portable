#pragma once

#include <SDL3/SDL.h>

#define LOG_CATEGORY_MINIWIN (SDL_LOG_CATEGORY_CUSTOM)

#define MINIWIN_TRACE(FUNCTION, MSG, ...)                                                                              \
	do {                                                                                                               \
		SDL_LogTrace(LOG_CATEGORY_MINIWIN, FUNCTION);                                                                  \
	}

#define MINIWIN_ERROR(MSG)                                                                                             \
	do {                                                                                                               \
		SDL_LogError(LOG_CATEGORY_MINIWIN, "%s:%s", __func__, MSG);                                                    \
	} while (0)

static SDL_FRect ConvertRect(const RECT* r)
{
	SDL_FRect sdlRect;
	sdlRect.x = r->left;
	sdlRect.y = r->top;
	sdlRect.w = r->right - r->left;
	sdlRect.h = r->bottom - r->top;
	return sdlRect;
}
