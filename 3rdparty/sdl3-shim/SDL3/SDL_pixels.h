#pragma once

#include <SDL2/SDL_pixels.h>

// https://wiki.libsdl.org/SDL3/README-migration#sdl_pixelsh
#define bits_per_pixel BitsPerPixel

typedef SDL_PixelFormat SDL2_PixelFormat;
#define SDL_PixelFormatDetails SDL2_PixelFormat
#define SDL_PixelFormat SDL_PixelFormatEnum

#define SDL_GetRGBA(pixel, format, palette, r,g,b,a) SDL_GetRGBA(pixel, format, r,g,b,a)
#define SDL_MapRGBA(format, palette, r,g,b,a) SDL_MapRGBA(format, r,g,b,a)
#define SDL_GetRGB(pixel, format, palette, r,g,b) SDL_GetRGB(pixel, format, r,g,b)
#define SDL_MapRGB(format, palette, r,g,b) SDL_MapRGB(format, r,g,b)

template<typename T>
SDL_PixelFormatDetails* SDL_GetPixelFormatDetails(T format) {
	if constexpr (std::is_same_v<T, SDL_PixelFormat>) {
		return SDL_AllocFormat(format);
	} else {
		return SDL_AllocFormat(format->format);
	}
}


static bool operator!=(SDL_PixelFormatDetails* lhs, SDL_PixelFormatEnum rhs)
{
	return lhs->format != rhs;
}

#define SDL_CreatePalette SDL_AllocPalette
#define SDL_DestroyPalette SDL_FreePalette

// #define SDL_GetPixelFormatForMasks (SDL_PixelFormat)SDL_MasksToPixelFormatEnum
inline SDL_PixelFormatEnum SDL_GetPixelFormatForMasks(int bpp, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
	// SDL2 checks if Rmask == 0; SDL3 just returns INDEX8 if Rmask doesn't match
	// https://github.com/libsdl-org/SDL/blob/66d87bf0e1e29377b398d3c567e1ab3590760d8c/src/video/SDL_pixels.c#L310C5-L321C13
	// https://github.com/libsdl-org/SDL/blob/3669920fddcc418c5f9aca97e77a3f380308d9c0/src/video/SDL_pixels.c#L411-L418C16
	if (bpp == 8 && Rmask != 0xE0) {
		return SDL_PIXELFORMAT_INDEX8;
	}
	return static_cast<SDL_PixelFormatEnum>(SDL_MasksToPixelFormatEnum(bpp, Rmask, Gmask, Bmask, Amask));
}

#define SDL_GetSurfacePalette(surface) (nullptr)
