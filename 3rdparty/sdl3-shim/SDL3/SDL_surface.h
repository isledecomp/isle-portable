#pragma once

#include <SDL_surface.h>
#include "SDL_pixels.h"

// https://wiki.libsdl.org/SDL3/README-migration#sdl_surfaceh
struct SDL_SurfaceShim : SDL_Surface {
	SDL_PixelFormat format;

	explicit SDL_SurfaceShim(const SDL_Surface* s)
		: SDL_Surface(*s), format(static_cast<SDL_PixelFormat>(s->format->format)) {}
};
#define SDL_Surface SDL_SurfaceShim

#define SDL_FillSurfaceRect(...) (SDL_FillRect(__VA_ARGS__) == 0)

// #define SDL_ConvertSurface(...) SDL_ConvertSurface(__VA_ARGS__, 0)

#define SDL_DestroySurface SDL_FreeSurface

#define SDL_CreateSurface(width, height, format) (SDL_Surface*)SDL_CreateRGBSurfaceWithFormat(0 , width, height, SDL_BITSPERPIXEL(format) ,format)

inline SDL_Surface* SDL_ConvertSurface(SDL_Surface* surface, SDL_PixelFormat format)
{
	const SDL_PixelFormatDetails* formatDetails = SDL_AllocFormat(format);
	return static_cast<SDL_Surface*>(SDL_ConvertSurface(surface, formatDetails, 0));
};

#define SDL_SCALEMODE_LINEAR SDL_ScaleModeLinear
#define SDL_SCALEMODE_NEAREST SDL_ScaleModeNearest

#define SDL_BlitSurfaceScaled(surface, rect, destSurface, destRect, scaleMode) (SDL_BlitScaled(surface, rect, destSurface, destRect) == 0)
#define SDL_SetSurfaceColorKey SDL_SetColorKey

#define SDL_LoadBMP_IO (SDL_Surface*)SDL_LoadBMP_RW
#define SDL_LoadBMP(file)  (SDL_Surface*)SDL_LoadBMP_RW(SDL_RWFromFile(file, "rb"), 1) //yoinked the existing SDL_LoadBMP macro so it could be cast
