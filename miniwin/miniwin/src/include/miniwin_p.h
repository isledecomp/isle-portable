#pragma once

#include <SDL3/SDL.h>

#define LOG_CATEGORY_MINIWIN (SDL_LOG_CATEGORY_CUSTOM)

#ifdef _MSC_VER
#define MINIWIN_PRETTY_FUNCTION __FUNCSIG__
#else
#define MINIWIN_PRETTY_FUNCTION __PRETTY_FUNCTION__
#endif

#define MINIWIN_NOT_IMPLEMENTED()                                                                                      \
	do {                                                                                                               \
		static bool visited = false;                                                                                   \
		if (!visited) {                                                                                                \
			SDL_LogError(LOG_CATEGORY_MINIWIN, "%s: Not implemented", MINIWIN_PRETTY_FUNCTION);                        \
			visited = true;                                                                                            \
		}                                                                                                              \
	} while (0)

#define MINIWIN_ERROR(MSG)                                                                                             \
	do {                                                                                                               \
		SDL_LogError(LOG_CATEGORY_MINIWIN, "%s: %s", __func__, MSG);                                                   \
	} while (0)

#define MINIWIN_TRACE(...)                                                                                             \
	do {                                                                                                               \
		SDL_LogTrace(LOG_CATEGORY_MINIWIN, __VA_ARGS__);                                                               \
	} while (0)
static SDL_Rect ConvertRect(const RECT* r)
{
	return {r->left, r->top, r->right - r->left, r->bottom - r->top};
}
