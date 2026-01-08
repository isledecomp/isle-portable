#include "mortar/mortar_surface.h"
#include "sdl3_internal.h"

#include <assert.h>
#include <stdlib.h>

// SDL_SCALECMODE_PIXELART requires SDL3>=3.3.2
#define COMPAT_SDL_SCALEMODE_PIXELART ((SDL_ScaleMode) 2)

typedef struct MORTAR_SDL_Surface {
	MORTAR_Surface mortar;
	SDL_Surface* sdl3_surface;
	MORTAR_SDL_Palette* mortar_palette;
} MORTAR_SDL_Surface;

static SDL_ScaleMode scalemode_mortar_to_sdl3(MORTAR_ScaleMode scaleMode)
{
	switch (scaleMode) {
	case MORTAR_SCALEMODE_NEAREST:
		return SDL_SCALEMODE_NEAREST;
	case MORTAR_SCALEMODE_LINEAR:
		return SDL_SCALEMODE_LINEAR;
	case MORTAR_SCALEMODE_PIXELART:
		if (SDL3_versionnum >= SDL_VERSIONNUM(3, 3, 2)) {
			return COMPAT_SDL_SCALEMODE_PIXELART;
		}
		else {
			return SDL_SCALEMODE_NEAREST;
		}
	default:
		abort();
	}
}

SDL_Surface* surface_mortar_to_sdl3(MORTAR_Surface* mortar_surface)
{
	if (!mortar_surface) {
		return nullptr;
	}
	MORTAR_SDL_Surface* mortar_sdl_surface = (MORTAR_SDL_Surface*) mortar_surface;
	return mortar_sdl_surface->sdl3_surface;
}

SDL_Palette* palette_mortar_to_sdl3(const MORTAR_Palette* mortar_palette)
{
	MORTAR_SDL_Palette* mortar_sdl_palette = (MORTAR_SDL_Palette*) mortar_palette;
	if (!mortar_palette) {
		return nullptr;
	}
	return mortar_sdl_palette->sdl3_palette;
}

static MORTAR_Surface* create_mortar_surface_from_sdl3(SDL_Surface* sdl3_surface)
{
	if (!sdl3_surface) {
		return NULL;
	}
	MORTAR_SDL_Surface* mortar_surface = (MORTAR_SDL_Surface*) SDL_calloc(1, sizeof(MORTAR_SDL_Surface));
	if (!mortar_surface) {
		SDL_DestroySurface(sdl3_surface);
		return nullptr;
	}
	mortar_surface->mortar.w = sdl3_surface->w;
	mortar_surface->mortar.h = sdl3_surface->h;
	mortar_surface->mortar.pitch = sdl3_surface->pitch;
	mortar_surface->mortar.pixels = sdl3_surface->pixels;
	mortar_surface->mortar.format = pixelformat_sdl3_to_mortar(sdl3_surface->format);
	mortar_surface->sdl3_surface = sdl3_surface;
	return &mortar_surface->mortar;
}

MORTAR_Surface* MORTAR_CreateSurface(int width, int height, MORTAR_PixelFormat format)
{
	return create_mortar_surface_from_sdl3(SDL_CreateSurface(width, height, pixelformat_mortar_to_sdl3(format)));
}

MORTAR_Surface* MORTAR_CreateSurfaceFrom(int width, int height, MORTAR_PixelFormat format, void* pixels, int pitch)
{
	return create_mortar_surface_from_sdl3(
		SDL_CreateSurfaceFrom(width, height, pixelformat_mortar_to_sdl3(format), pixels, pitch)
	);
}

void MORTAR_DestroySurface(MORTAR_Surface* mortar_surface)
{
	MORTAR_SDL_Surface* mortar_sdl_surface = (MORTAR_SDL_Surface*) mortar_surface;
	if (mortar_sdl_surface->mortar_palette) {
		MORTAR_DestroyPalette(&mortar_sdl_surface->mortar_palette->mortar);
	}
	SDL_DestroySurface(surface_mortar_to_sdl3(mortar_surface));
	SDL_free(mortar_sdl_surface);
}

bool MORTAR_LockSurface(MORTAR_Surface* mortar_surface)
{
	return SDL_LockSurface(surface_mortar_to_sdl3(mortar_surface));
}

void MORTAR_UnlockSurface(MORTAR_Surface* mortar_surface)
{
	return SDL_UnlockSurface(surface_mortar_to_sdl3(mortar_surface));
}

bool MORTAR_FillSurfaceRect(MORTAR_Surface* dst, const MORTAR_Rect* rect, uint32_t color)
{
	SDL_Rect sdl3_rect_storage;
	SDL_Rect* sdl3_rect_ptr = rect_mortar_to_sdl3(rect, &sdl3_rect_storage);
	return SDL_FillSurfaceRect(surface_mortar_to_sdl3(dst), sdl3_rect_ptr, color);
}

bool MORTAR_BlitSurface(
	MORTAR_Surface* mortar_src,
	const MORTAR_Rect* mortar_srcrect,
	MORTAR_Surface* mortar_dst,
	const MORTAR_Rect* mortar_dstrect
)
{
	SDL_Rect sdl3_srcrect_storage;
	SDL_Rect* sdl3_srcrect = rect_mortar_to_sdl3(mortar_srcrect, &sdl3_srcrect_storage);
	SDL_Rect sdl3_dstrect_storage;
	SDL_Rect* sdl3_dstrect = rect_mortar_to_sdl3(mortar_dstrect, &sdl3_dstrect_storage);
	return SDL_BlitSurface(
		surface_mortar_to_sdl3(mortar_src),
		sdl3_srcrect,
		surface_mortar_to_sdl3(mortar_dst),
		sdl3_dstrect
	);
}

bool MORTAR_SetSurfacePalette(MORTAR_Surface* mortar_surface, MORTAR_Palette* mortar_palette)
{
	if (!mortar_surface) {
		return false;
	}
	MORTAR_SDL_Surface* mortar_sdl_surface = (MORTAR_SDL_Surface*) mortar_surface;
	if (mortar_sdl_surface->mortar_palette && &mortar_sdl_surface->mortar_palette->mortar == mortar_palette) {
		return true;
	}
	if (mortar_sdl_surface->mortar_palette) {
		MORTAR_DestroyPalette(&mortar_sdl_surface->mortar_palette->mortar);
		mortar_sdl_surface->mortar_palette = NULL;
	}
	bool result = SDL_SetSurfacePalette(mortar_sdl_surface->sdl3_surface, palette_mortar_to_sdl3(mortar_palette));
	mortar_sdl_surface->mortar_palette = (MORTAR_SDL_Palette*) mortar_palette;
	if (mortar_sdl_surface->mortar_palette) {
		mortar_sdl_surface->mortar_palette->sdl3_palette->refcount += 1;
		mortar_sdl_surface->mortar_palette->refcount += 1;
	}
	return result;
}

MORTAR_Palette* MORTAR_GetSurfacePalette(MORTAR_Surface* mortar_surface)
{
	MORTAR_SDL_Surface* mortar_sdl_surface = (MORTAR_SDL_Surface*) mortar_surface;
	if (mortar_sdl_surface == NULL) {
		return NULL;
	}
	if (mortar_sdl_surface->mortar_palette == NULL) {
		return NULL;
	}
	return &mortar_sdl_surface->mortar_palette->mortar;
}

MORTAR_Surface* MORTAR_ConvertSurface(MORTAR_Surface* mortar_surface, MORTAR_PixelFormat format)
{
	MORTAR_Surface* mortar_result_surface = create_mortar_surface_from_sdl3(
		SDL_ConvertSurface(surface_mortar_to_sdl3(mortar_surface), pixelformat_mortar_to_sdl3(format))
	);
	if (mortar_result_surface && mortar_result_surface->format == MORTAR_PIXELFORMAT_INDEX8) {
		MORTAR_SDL_Surface* mortar_sdl_surface = (MORTAR_SDL_Surface*) mortar_surface;
		MORTAR_SDL_Surface* mortar_result_sdl_surface = (MORTAR_SDL_Surface*) mortar_result_surface;
		mortar_result_sdl_surface->mortar_palette = mortar_sdl_surface->mortar_palette;
		if (mortar_result_sdl_surface->mortar_palette) {
			mortar_result_sdl_surface->mortar_palette->sdl3_palette->refcount += 1;
			mortar_result_sdl_surface->mortar_palette->refcount += 1;
		}
	}
	return mortar_result_surface;
}

bool MORTAR_BlitSurfaceScaled(
	MORTAR_Surface* mortar_src,
	const MORTAR_Rect* mortar_srcrect,
	MORTAR_Surface* mortar_dst,
	const MORTAR_Rect* mortar_dstrect,
	MORTAR_ScaleMode mortar_scaleMode
)
{
	SDL_Rect sdl3_srcrect_storage;
	SDL_Rect sdl3_dstrect_storage;
	return SDL_BlitSurfaceScaled(
		surface_mortar_to_sdl3(mortar_src),
		rect_mortar_to_sdl3(mortar_srcrect, &sdl3_srcrect_storage),
		surface_mortar_to_sdl3(mortar_dst),
		rect_mortar_to_sdl3(mortar_dstrect, &sdl3_dstrect_storage),
		scalemode_mortar_to_sdl3(mortar_scaleMode)
	);
}

bool MORTAR_SetSurfaceColorKey(MORTAR_Surface* mortar_surface, bool enabled, uint32_t key)
{
	return SDL_SetSurfaceColorKey(surface_mortar_to_sdl3(mortar_surface), enabled, key);
}

MORTAR_Surface* MORTAR_LoadBMP(const char* file)
{
	return create_mortar_surface_from_sdl3(SDL_LoadBMP(file));
}

MORTAR_Surface* MORTAR_LoadBMP_IO(MORTAR_IOStream* src, bool closeio)
{
	return create_mortar_surface_from_sdl3(SDL_LoadBMP_IO((SDL_IOStream*) src, closeio));
}

void* MORTAR_EXT_GetSurfaceProperty(MORTAR_Surface* mortar_surface, MORTAR_SurfaceProperty key, void* default_value)
{
	switch (key) {
	case MORTAR_SURFACE_PROPERTY_SDL3SURFACE:
		return surface_mortar_to_sdl3(mortar_surface);
	default:
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "MORTAR_EXT_GetSurfaceProperty: unsupported property");
		return default_value;
	}
}
