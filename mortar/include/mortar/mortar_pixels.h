#pragma once

#include "mortar_begin.h"

#include <cstdint>

typedef struct MORTAR_Color {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
} MORTAR_Color;

#define MORTAR_ALPHA_OPAQUE 255

typedef enum MORTAR_PixelFormat {
	MORTAR_PIXELFORMAT_UNKNOWN = 0,
	MORTAR_PIXELFORMAT_INDEX1LSB,
	MORTAR_PIXELFORMAT_INDEX1MSB,
	MORTAR_PIXELFORMAT_INDEX8,
	MORTAR_PIXELFORMAT_RGBA32,
	MORTAR_PIXELFORMAT_RGBX32,
	MORTAR_PIXELFORMAT_BGR24,
	MORTAR_PIXELFORMAT_XRGB32,
	MORTAR_PIXELFORMAT_ARGB32,
	MORTAR_PIXELFORMAT_RGBX8888,
	MORTAR_PIXELFORMAT_XBGR8888,
	MORTAR_PIXELFORMAT_BGRX8888,
	MORTAR_PIXELFORMAT_XRGB8888,
	MORTAR_PIXELFORMAT_ARGB8888,
	MORTAR_PIXELFORMAT_BGRA8888,
	MORTAR_PIXELFORMAT_RGBA8888,
	MORTAR_PIXELFORMAT_ABGR8888,
} MORTAR_PixelFormat;

typedef struct MORTAR_Palette {
	int ncolors;          /**< number of elements in `colors`. */
	MORTAR_Color* colors; /**< an array of colors, `ncolors` long. */
} MORTAR_Palette;

typedef struct MORTAR_PixelFormatDetails {
	MORTAR_PixelFormat format;
	uint8_t bits_per_pixel;
	uint32_t Rmask;
	uint32_t Gmask;
	uint32_t Bmask;
	uint32_t Amask;
	void* reserved;
} MORTAR_PixelFormatDetails;

extern MORTAR_DECLSPEC MORTAR_Palette* MORTAR_CreatePalette(int ncolors);

extern MORTAR_DECLSPEC void MORTAR_DestroyPalette(MORTAR_Palette* palette);

extern MORTAR_DECLSPEC bool MORTAR_SetPaletteColors(
	MORTAR_Palette* palette,
	const MORTAR_Color* colors,
	int firstcolor,
	int ncolors
);

extern MORTAR_DECLSPEC const MORTAR_PixelFormatDetails* MORTAR_GetPixelFormatDetails(MORTAR_PixelFormat format);

extern MORTAR_DECLSPEC uint32_t MORTAR_MapRGBA(
	const MORTAR_PixelFormatDetails* format,
	const MORTAR_Palette* palette,
	uint8_t r,
	uint8_t g,
	uint8_t b,
	uint8_t a
);

extern MORTAR_DECLSPEC uint32_t
MORTAR_MapRGB(const MORTAR_PixelFormatDetails* format, const MORTAR_Palette* palette, uint8_t r, uint8_t g, uint8_t b);

extern MORTAR_DECLSPEC void MORTAR_GetRGBA(
	uint32_t pixelvalue,
	const MORTAR_PixelFormatDetails* format,
	const MORTAR_Palette* palette,
	uint8_t* r,
	uint8_t* g,
	uint8_t* b,
	uint8_t* a
);

extern MORTAR_DECLSPEC MORTAR_PixelFormat
MORTAR_GetPixelFormatForMasks(int bpp, uint32_t Rmask, uint32_t Gmask, uint32_t Bmask, uint32_t Amask);

extern MORTAR_DECLSPEC const char* MORTAR_GetPixelFormatName(MORTAR_PixelFormat format);

#include "mortar_end.h"
