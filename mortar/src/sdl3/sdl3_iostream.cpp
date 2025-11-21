#include "mortar/mortar_iostream.h"
#include "sdl3_internal.h"

#include <stdlib.h>

static SDL_IOWhence iowhence_mortar_to_sdl3(MORTAR_IOWhence whence)
{
	switch (whence) {
	case MORTAR_IO_SEEK_SET:
		return SDL_IO_SEEK_SET;
	case MORTAR_IO_SEEK_CUR:
		return SDL_IO_SEEK_CUR;
	case MORTAR_IO_SEEK_END:
		return SDL_IO_SEEK_END;
	default:
		abort();
	}
}

static MORTAR_IOStatus iostatus_sdl3_to_mortar(SDL_IOStatus status)
{
	switch (status) {
	case SDL_IO_STATUS_READY:
		return MORTAR_IO_STATUS_READY;
	case SDL_IO_STATUS_ERROR:
		return MORTAR_IO_STATUS_ERROR;
	case SDL_IO_STATUS_EOF:
		return MORTAR_IO_STATUS_EOF;
	case SDL_IO_STATUS_NOT_READY:
		return MORTAR_IO_STATUS_NOT_READY;
	case SDL_IO_STATUS_READONLY:
		return MORTAR_IO_STATUS_READONLY;
	case SDL_IO_STATUS_WRITEONLY:
		return MORTAR_IO_STATUS_WRITEONLY;
	default:
		abort();
	}
}

SDL_IOStream* iostream_mortar_to_sdl3(MORTAR_IOStream* mortar_stream)
{
	return (SDL_IOStream*) mortar_stream;
}

MORTAR_IOStream* MORTAR_IOFromFile(const char* file, const char* mode)
{
	return (MORTAR_IOStream*) SDL_IOFromFile(file, mode);
}

MORTAR_IOStream* MORTAR_IOFromMem(void* mem, size_t size)
{
	return (MORTAR_IOStream*) SDL_IOFromMem(mem, size);
}

size_t MORTAR_ReadIO(MORTAR_IOStream* context, void* ptr, size_t size)
{
	return SDL_ReadIO(iostream_mortar_to_sdl3(context), ptr, size);
}

size_t MORTAR_WriteIO(MORTAR_IOStream* context, const void* ptr, size_t size)
{
	return SDL_WriteIO(iostream_mortar_to_sdl3(context), ptr, size);
}

int64_t MORTAR_SeekIO(MORTAR_IOStream* context, int64_t offset, MORTAR_IOWhence whence)
{
	return SDL_SeekIO(iostream_mortar_to_sdl3(context), offset, iowhence_mortar_to_sdl3(whence));
}

int64_t MORTAR_TellIO(MORTAR_IOStream* context)
{
	return SDL_TellIO(iostream_mortar_to_sdl3(context));
}

int64_t MORTAR_GetIOSize(MORTAR_IOStream* context)
{
	return SDL_GetIOSize(iostream_mortar_to_sdl3(context));
}

bool MORTAR_CloseIO(MORTAR_IOStream* context)
{
	return SDL_CloseIO(iostream_mortar_to_sdl3(context));
}

MORTAR_IOStatus MORTAR_GetIOStatus(MORTAR_IOStream* context)
{
	return iostatus_sdl3_to_mortar(SDL_GetIOStatus(iostream_mortar_to_sdl3(context)));
}

void* MORTAR_LoadFile(const char* file, size_t* datasize)
{
	return SDL_LoadFile(file, datasize);
}
