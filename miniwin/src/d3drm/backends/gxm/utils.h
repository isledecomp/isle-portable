#pragma once

#include <SDL3/SDL_log.h>

static bool _sce_err(const char* expr, int err) {
	if(err >= 0) {
		SDL_Log("sce: %s", expr);
		return false;
	}
	SDL_Log("SCE_ERR: %s failed 0x%x", expr, err);
	return true;
}

#define SCE_ERR(func, ...) _sce_err(#func, func(__VA_ARGS__))

#define ALIGN(x, a) (((x) + ((a)-1)) & ~((a)-1))

#define INCBIN(filename, symbol) \
	__asm__( \
		".balign 16 \n" \
		#symbol ":" \
		".incbin \"" filename "\"" \
	); \
	extern const void* symbol
