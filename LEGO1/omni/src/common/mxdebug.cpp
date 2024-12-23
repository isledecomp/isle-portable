#include "mxdebug.h"

#ifdef _DEBUG

// Debug-only wrapper for OutputDebugString to support variadic arguments.
// Identical functions at BETA10 0x100ec9fe and 0x101741b5 are more limited in scope.
// This is the most widely used version.

#include <stdio.h>
#include <SDL3/SDL_log.h>

// FUNCTION: BETA10 0x10124cb9
int DebugHeapState()
{
	return 0;
}

// FUNCTION: BETA10 0x10124cdd
void _MxTrace(const char* format, ...)
{
	va_list args;

	va_start(args, format);
	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_CATEGORY_DEBUG, SDL_LOG_PRIORITY_TRACE, format, args);
	va_end(args);
}

#endif
