#pragma once

#include <mortar/mortar.h>

#define LOG_CATEGORY_MINIWIN (MORTAR_LOG_CATEGORY_CUSTOM)

#ifdef _MSC_VER
#define MINIWIN_PRETTY_FUNCTION __FUNCSIG__
#else
#define MINIWIN_PRETTY_FUNCTION __PRETTY_FUNCTION__
#endif

#define MINIWIN_NOT_IMPLEMENTED()                                                                                      \
	do {                                                                                                               \
		static bool visited = false;                                                                                   \
		if (!visited) {                                                                                                \
			MORTAR_LogError(LOG_CATEGORY_MINIWIN, "%s: Not implemented", MINIWIN_PRETTY_FUNCTION);                     \
			visited = true;                                                                                            \
		}                                                                                                              \
	} while (0)

#define MINIWIN_ERROR(MSG)                                                                                             \
	do {                                                                                                               \
		MORTAR_LogError(LOG_CATEGORY_MINIWIN, "%s: %s", __func__, MSG);                                                \
	} while (0)

#define MINIWIN_TRACE(...)                                                                                             \
	do {                                                                                                               \
		MORTAR_LogTrace(LOG_CATEGORY_MINIWIN, __VA_ARGS__);                                                            \
	} while (0)
