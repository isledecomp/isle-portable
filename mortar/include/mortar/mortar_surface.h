#pragma once

#include "mortar/mortar_iostream.h"
#include "mortar/mortar_pixels.h"
#include "mortar/mortar_rect.h"
#include "mortar_begin.h"

typedef struct MORTAR_Surface {
	//	SDL_SurfaceFlags flags;     /**< The flags of the surface, read-only */
	MORTAR_PixelFormat format;
	int w;
	int h;
	int pitch;    /**< The distance in bytes between rows of pixels, read-only */
	void* pixels; /**< A pointer to the pixels of the surface, the pixels are writeable if non-NULL */

	//	int refcount;               /**< Application reference count, used when freeing surface */
	//
	//	void *reserved;             /**< Reserved for internal use */
} MORTAR_Surface;

typedef enum MORTAR_ScaleMode {
	MORTAR_SCALEMODE_NEAREST,
	MORTAR_SCALEMODE_LINEAR,
	MORTAR_SCALEMODE_PIXELART,
} MORTAR_ScaleMode;

typedef enum MORTAR_SurfaceProperty {
	MORTAR_SURFACE_PROPERTY_SDL3SURFACE,
} MORTAR_SurfaceProperty;

extern MORTAR_DECLSPEC MORTAR_Surface* MORTAR_CreateSurface(int width, int height, MORTAR_PixelFormat format);

extern MORTAR_DECLSPEC MORTAR_Surface* MORTAR_CreateSurfaceFrom(
	int width,
	int height,
	MORTAR_PixelFormat format,
	void* pixels,
	int pitch
);

extern MORTAR_DECLSPEC void MORTAR_DestroySurface(MORTAR_Surface* surface);

extern MORTAR_DECLSPEC bool MORTAR_LockSurface(MORTAR_Surface* surface);

extern MORTAR_DECLSPEC void MORTAR_UnlockSurface(MORTAR_Surface* surface);

extern MORTAR_DECLSPEC bool MORTAR_FillSurfaceRect(MORTAR_Surface* dst, const MORTAR_Rect* rect, uint32_t color);

extern MORTAR_DECLSPEC bool MORTAR_BlitSurface(
	MORTAR_Surface* src,
	const MORTAR_Rect* srcrect,
	MORTAR_Surface* dst,
	const MORTAR_Rect* dstrect
);

extern MORTAR_DECLSPEC bool MORTAR_SetSurfacePalette(MORTAR_Surface* surface, MORTAR_Palette* palette);

extern MORTAR_DECLSPEC MORTAR_Palette* MORTAR_GetSurfacePalette(MORTAR_Surface* surface);

extern MORTAR_DECLSPEC MORTAR_Surface* MORTAR_ConvertSurface(MORTAR_Surface* surface, MORTAR_PixelFormat format);

extern MORTAR_DECLSPEC bool MORTAR_BlitSurfaceScaled(
	MORTAR_Surface* src,
	const MORTAR_Rect* srcrect,
	MORTAR_Surface* dst,
	const MORTAR_Rect* dstrect,
	MORTAR_ScaleMode scaleMode
);

extern MORTAR_DECLSPEC bool MORTAR_SetSurfaceColorKey(MORTAR_Surface* surface, bool enabled, uint32_t key);

extern MORTAR_DECLSPEC MORTAR_Surface* MORTAR_LoadBMP(const char* file);

extern MORTAR_DECLSPEC MORTAR_Surface* MORTAR_LoadBMP_IO(MORTAR_IOStream* src, bool closeio);

extern MORTAR_DECLSPEC void* MORTAR_EXT_GetSurfaceProperty(
	MORTAR_Surface* mortar_surface,
	MORTAR_SurfaceProperty key,
	void* default_value
);

#include "mortar_end.h"
