#pragma once

#include <SDL2/SDL_surface.h>
#include "SDL_pixels.h"

// https://wiki.libsdl.org/SDL3/README-migration#sdl_surfaceh

#define SDL_FillSurfaceRect(...) (SDL_FillRect(__VA_ARGS__) == 0)

#define SDL_DestroySurface SDL_FreeSurface

#define SDL_LockSurface(...) (SDL_LockSurface(__VA_ARGS__) == 0)

template<typename T>
SDL_Surface* SDL_CreateSurface( int width, int height, T format) {
	if constexpr (std::is_same_v<T, SDL_PixelFormat>) {
		return SDL_CreateRGBSurfaceWithFormat(0 , width, height, SDL_BITSPERPIXEL(format) ,format);
	} else {
		return SDL_CreateRGBSurfaceWithFormat(0 , width, height, SDL_BITSPERPIXEL(format->format) ,format->format);
	}
}

inline SDL_Surface* SDL_ConvertSurface(SDL_Surface* surface, SDL_PixelFormat format)
{
	return SDL_ConvertSurfaceFormat(surface, format, 0);
};
inline SDL_Surface* SDL_ConvertSurface(SDL_Surface* surface, const SDL_PixelFormatDetails* formatDetails)
{
	return SDL_ConvertSurface(surface, formatDetails, 0);
}

#define SDL_SCALEMODE_LINEAR SDL_ScaleModeLinear
#define SDL_SCALEMODE_NEAREST SDL_ScaleModeNearest

#define SDL_BlitSurfaceScaled(surface, rect, destSurface, destRect, scaleMode) (SDL_BlitScaled(surface, rect, destSurface, destRect) == 0)
#define SDL_SetSurfaceColorKey(...) (SDL_SetColorKey(__VA_ARGS__) == 0)

#define SDL_LoadBMP_IO SDL_LoadBMP_RW
