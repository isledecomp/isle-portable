#pragma once

#include <SDL2/SDL_rwops.h>

// https://wiki.libsdl.org/SDL3/README-migration#sdl_rwopsh

#define SDL_IOStream SDL_RWops

typedef int SDL_IOWhence;

#define SDL_IO_SEEK_SET RW_SEEK_SET
#define SDL_IO_SEEK_CUR RW_SEEK_CUR
#define SDL_IO_SEEK_END RW_SEEK_END

// #define SDL_IOFromConstMem SDL_RWFromConstMem
#define SDL_IOFromFile SDL_RWFromFile
#define SDL_IOFromMem SDL_RWFromMem
#define SDL_CloseIO SDL_RWclose
#define SDL_ReadIO(ctx, ptr, size) SDL_RWread(ctx, ptr, 1, size)
#define SDL_SeekIO SDL_RWseek
#define SDL_GetIOSize SDL_RWsize
#define SDL_TellIO SDL_RWtell
#define SDL_WriteIO(ctx, ptr, size) SDL_RWwrite(ctx, ptr, 1, size)
// #define SDL_ReadU16BE SDL_ReadBE16
// #define SDL_ReadU32BE SDL_ReadBE32
// #define SDL_ReadU64BE SDL_ReadBE64
// #define SDL_ReadU16LE SDL_ReadLE16
// #define SDL_ReadU32LE SDL_ReadLE32
// #define SDL_ReadU64LE SDL_ReadLE64
// #define SDL_WriteU16BE SDL_WriteBE16
// #define SDL_WriteU32BE SDL_WriteBE32
// #define SDL_WriteU64BE SDL_WriteBE64
// #define SDL_WriteU16LE SDL_WriteLE16
// #define SDL_WriteU32LE SDL_WriteLE32
// #define SDL_WriteU64LE SDL_WriteLE64

// FIXME: If Write/Read fail SDL_GetIOStatus is not aware.
typedef enum SDL_IOStatus
{
	SDL_IO_STATUS_READY,
	SDL_IO_STATUS_ERROR,
} SDL_IOStatus;
inline SDL_IOStatus SDL_GetIOStatus(SDL_RWops *context)
{
	if (!context) {
		return SDL_IO_STATUS_ERROR;
	}
	return SDL_IO_STATUS_READY;
}
