#pragma once

#include <SDL_pixels.h>

// https://wiki.libsdl.org/SDL3/README-migration#sdl_pixelsh
#define bits_per_pixel BitsPerPixel

typedef SDL_PixelFormat SDL2_PixelFormat;
#define SDL_PixelFormatDetails SDL2_PixelFormat
#define SDL_PixelFormat SDL_PixelFormatEnum

#define SDL_GetRGBA(pixel, format, palette, r,g,b,a) SDL_GetRGBA(pixel, format, r,g,b,a)
#define SDL_MapRGBA(format, palette, r,g,b,a) SDL_MapRGBA(format, r,g,b,a)
#define SDL_GetRGB(pixel, format, palette, r,g,b) SDL_GetRGB(pixel, format, r,g,b)
#define SDL_MapRGB(format, palette, r,g,b) SDL_MapRGB(format, r,g,b)

#define SDL_GetPixelFormatDetails SDL_AllocFormat

#define SDL_CreatePalette SDL_AllocPalette
#define SDL_DestroyPalette SDL_FreePalette

#define SDL_GetPixelFormatForMasks (SDL_PixelFormat)SDL_MasksToPixelFormatEnum

#define SDL_GetSurfacePalette(surface) (nullptr)
