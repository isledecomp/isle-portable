#include "mortar/mortar_log.h"
#include "sdl3_internal.h"

#include <stdarg.h>
#include <stdlib.h>

static int category_mortar_to_sdl3(int category)
{
	switch (category) {
	case MORTAR_LOG_CATEGORY_APPLICATION:
		return SDL_LOG_CATEGORY_APPLICATION;
	default:
		abort();
	}
}

static int priority_mortar_to_sdl3(int priority)
{
	switch (priority) {
	case MORTAR_LOG_PRIORITY_INVALID:
		return SDL_LOG_PRIORITY_INVALID;
	case MORTAR_LOG_PRIORITY_TRACE:
		return SDL_LOG_PRIORITY_TRACE;
	case MORTAR_LOG_PRIORITY_VERBOSE:
		return SDL_LOG_PRIORITY_VERBOSE;
	case MORTAR_LOG_PRIORITY_DEBUG:
		return SDL_LOG_PRIORITY_DEBUG;
	case MORTAR_LOG_PRIORITY_INFO:
		return SDL_LOG_PRIORITY_INFO;
	case MORTAR_LOG_PRIORITY_WARN:
		return SDL_LOG_PRIORITY_WARN;
	case MORTAR_LOG_PRIORITY_ERROR:
		return SDL_LOG_PRIORITY_ERROR;
	case MORTAR_LOG_PRIORITY_CRITICAL:
		return SDL_LOG_PRIORITY_CRITICAL;
	default:
		abort();
	}
}

void MORTAR_LogError(int category, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	SDL_LogMessageV(category, SDL_LOG_PRIORITY_ERROR, fmt, ap);
	va_end(ap);
}

void MORTAR_LogWarn(int category, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	SDL_LogMessageV(category, SDL_LOG_PRIORITY_WARN, fmt, ap);
	va_end(ap);
}

void MORTAR_LogTrace(int category, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	SDL_LogMessageV(category, SDL_LOG_PRIORITY_TRACE, fmt, ap);
	va_end(ap);
}

void MORTAR_LogInfo(int category, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	SDL_LogMessageV(category, SDL_LOG_PRIORITY_INFO, fmt, ap);
	va_end(ap);
}

void MORTAR_Log(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, fmt, ap);
	va_end(ap);
}

void MORTAR_LogMessageV(int category, MORTAR_LogPriority priority, const char* fmt, va_list ap)
{
	SDL_LogMessageV(category_mortar_to_sdl3(category), SDL_LOG_PRIORITY_INFO, fmt, ap);
}
