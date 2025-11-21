#include "mortar/mortar_pixels.h"
#include "sdl3_internal.h"

#include <stdlib.h>
#include <unordered_map>

static bool g_pixelformatdetails_initialized;
static std::unordered_map<MORTAR_PixelFormat, MORTAR_PixelFormatDetails> g_pixelformatdetails;

static void insert_pixelformatdetails(MORTAR_PixelFormat pixelformat)
{
	SDL_PixelFormat sdl3_pixelformat = pixelformat_mortar_to_sdl3(pixelformat);
	const SDL_PixelFormatDetails* sdl3_details = SDL_GetPixelFormatDetails(sdl3_pixelformat);
	MORTAR_PixelFormatDetails new_details;
	new_details.format = pixelformat_sdl3_to_mortar(sdl3_details->format);
	new_details.bits_per_pixel = sdl3_details->bits_per_pixel;
	new_details.Rmask = sdl3_details->Rmask;
	new_details.Gmask = sdl3_details->Gmask;
	new_details.Bmask = sdl3_details->Bmask;
	new_details.Amask = sdl3_details->Amask;
	new_details.reserved = (void*) sdl3_details;
	g_pixelformatdetails[pixelformat] = new_details;
}

static void ensure_pixelformatdetails_initialized()
{
	if (!g_pixelformatdetails_initialized) {
		insert_pixelformatdetails(MORTAR_PIXELFORMAT_INDEX1MSB);
		insert_pixelformatdetails(MORTAR_PIXELFORMAT_INDEX1LSB);
		insert_pixelformatdetails(MORTAR_PIXELFORMAT_INDEX8);
		insert_pixelformatdetails(MORTAR_PIXELFORMAT_RGBA32);
		insert_pixelformatdetails(MORTAR_PIXELFORMAT_RGBX32);
		insert_pixelformatdetails(MORTAR_PIXELFORMAT_XRGB32);
		insert_pixelformatdetails(MORTAR_PIXELFORMAT_ARGB32);
		insert_pixelformatdetails(MORTAR_PIXELFORMAT_RGBX8888);
		insert_pixelformatdetails(MORTAR_PIXELFORMAT_XBGR8888);
		insert_pixelformatdetails(MORTAR_PIXELFORMAT_BGRX8888);
		insert_pixelformatdetails(MORTAR_PIXELFORMAT_XRGB8888);
		insert_pixelformatdetails(MORTAR_PIXELFORMAT_ARGB8888);
		insert_pixelformatdetails(MORTAR_PIXELFORMAT_BGRA8888);
		insert_pixelformatdetails(MORTAR_PIXELFORMAT_RGBA8888);
		insert_pixelformatdetails(MORTAR_PIXELFORMAT_ABGR8888);
		insert_pixelformatdetails(MORTAR_PIXELFORMAT_BGR24);
		g_pixelformatdetails_initialized = true;
	}
}

MORTAR_PixelFormat pixelformat_sdl3_to_mortar(SDL_PixelFormat format)
{
	SDL_assert(format != SDL_PIXELFORMAT_UNKNOWN);
	switch (format) {
	case SDL_PIXELFORMAT_INDEX1MSB:
		return MORTAR_PIXELFORMAT_INDEX1MSB;
	case SDL_PIXELFORMAT_INDEX1LSB:
		return MORTAR_PIXELFORMAT_INDEX1LSB;
	case SDL_PIXELFORMAT_INDEX8:
		return MORTAR_PIXELFORMAT_INDEX8;
	case SDL_PIXELFORMAT_RGBX8888:
		return MORTAR_PIXELFORMAT_RGBX8888;
	case SDL_PIXELFORMAT_XBGR8888:
		return MORTAR_PIXELFORMAT_XBGR8888;
	case SDL_PIXELFORMAT_BGRX8888:
		return MORTAR_PIXELFORMAT_BGRX8888;
	case SDL_PIXELFORMAT_XRGB8888:
		return MORTAR_PIXELFORMAT_XRGB8888;
	case SDL_PIXELFORMAT_ARGB8888:
		return MORTAR_PIXELFORMAT_ARGB8888;
	case SDL_PIXELFORMAT_BGRA8888:
		return MORTAR_PIXELFORMAT_BGRA8888;
	case SDL_PIXELFORMAT_RGBA8888:
		return MORTAR_PIXELFORMAT_RGBA8888;
	case SDL_PIXELFORMAT_ABGR8888:
		return MORTAR_PIXELFORMAT_ABGR8888;
	case SDL_PIXELFORMAT_BGR24:
		return MORTAR_PIXELFORMAT_BGR24;
	case SDL_PIXELFORMAT_UNKNOWN:
		return MORTAR_PIXELFORMAT_UNKNOWN;
	default:
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unsupported SDL pixelformat (%s)", SDL_GetPixelFormatName(format));
		abort();
	}
}

SDL_PixelFormat pixelformat_mortar_to_sdl3(MORTAR_PixelFormat format)
{
	SDL_assert(format != MORTAR_PIXELFORMAT_UNKNOWN);
	switch (format) {
	case MORTAR_PIXELFORMAT_INDEX1MSB:
		return SDL_PIXELFORMAT_INDEX1MSB;
	case MORTAR_PIXELFORMAT_INDEX1LSB:
		return SDL_PIXELFORMAT_INDEX1LSB;
	case MORTAR_PIXELFORMAT_INDEX8:
		return SDL_PIXELFORMAT_INDEX8;
	case MORTAR_PIXELFORMAT_RGBA32:
		return SDL_PIXELFORMAT_RGBA32;
	case MORTAR_PIXELFORMAT_RGBX32:
		return SDL_PIXELFORMAT_RGBX32;
	case MORTAR_PIXELFORMAT_XRGB32:
		return SDL_PIXELFORMAT_XRGB32;
	case MORTAR_PIXELFORMAT_ARGB32:
		return SDL_PIXELFORMAT_ARGB32;
	case MORTAR_PIXELFORMAT_RGBX8888:
		return SDL_PIXELFORMAT_RGBX8888;
	case MORTAR_PIXELFORMAT_XBGR8888:
		return SDL_PIXELFORMAT_XBGR8888;
	case MORTAR_PIXELFORMAT_BGRX8888:
		return SDL_PIXELFORMAT_BGRX8888;
	case MORTAR_PIXELFORMAT_XRGB8888:
		return SDL_PIXELFORMAT_XRGB8888;
	case MORTAR_PIXELFORMAT_ARGB8888:
		return SDL_PIXELFORMAT_ARGB8888;
	case MORTAR_PIXELFORMAT_BGRA8888:
		return SDL_PIXELFORMAT_BGRA8888;
	case MORTAR_PIXELFORMAT_RGBA8888:
		return SDL_PIXELFORMAT_RGBA8888;
	case MORTAR_PIXELFORMAT_ABGR8888:
		return SDL_PIXELFORMAT_ABGR8888;
	case MORTAR_PIXELFORMAT_BGR24:
		return SDL_PIXELFORMAT_BGR24;
	case MORTAR_PIXELFORMAT_UNKNOWN:
		return SDL_PIXELFORMAT_UNKNOWN;
	default:
		abort();
	}
}

SDL_Color color_mortar_to_sdl3(const MORTAR_Color* color)
{
	SDL_Color result;
	result.r = color->r;
	result.g = color->g;
	result.b = color->b;
	result.a = color->a;
	return result;
}

MORTAR_Palette* MORTAR_CreatePalette(int ncolors)
{
	MORTAR_SDL_Palette* mortar_sdl_palette = (MORTAR_SDL_Palette*) SDL_calloc(1, sizeof(MORTAR_SDL_Palette));
	if (!mortar_sdl_palette) {
		return nullptr;
	}
	SDL_Palette* sdl3_palette = SDL_CreatePalette(ncolors);
	if (!sdl3_palette) {
		SDL_free(mortar_sdl_palette);
		return nullptr;
	}
	SDL_assert(sdl3_palette->refcount == 1);
	sdl3_palette->refcount += 1;
	mortar_sdl_palette->refcount += 1;
	mortar_sdl_palette->mortar.ncolors = sdl3_palette->ncolors;
	mortar_sdl_palette->mortar.colors = (MORTAR_Color*) SDL_calloc(sizeof(MORTAR_Color), sdl3_palette->ncolors);
	mortar_sdl_palette->sdl3_palette = sdl3_palette;
	return &mortar_sdl_palette->mortar;
}

void MORTAR_DestroyPalette(MORTAR_Palette* mortar_palette)
{
	if (!mortar_palette) {
		return;
	}
	MORTAR_SDL_Palette* mortar_sdl_palette = (MORTAR_SDL_Palette*) mortar_palette;
	mortar_sdl_palette->sdl3_palette->refcount -= 1;
	mortar_sdl_palette->refcount -= 1;
	if (mortar_sdl_palette->refcount == 0) {
		SDL_DestroyPalette(mortar_sdl_palette->sdl3_palette);
		SDL_free(mortar_sdl_palette->mortar.colors);
		SDL_free(mortar_sdl_palette);
	}
}

bool MORTAR_SetPaletteColors(
	MORTAR_Palette* mortar_palette,
	const MORTAR_Color* mortar_colors,
	int firstcolor,
	int ncolors
)
{
	SDL_Color* sdl_colors = SDL_stack_alloc(SDL_Color, ncolors);
	for (int i = 0; i < ncolors; i++) {
		sdl_colors[i] = color_mortar_to_sdl3(&mortar_colors[i]);
	}
	bool result = SDL_SetPaletteColors(palette_mortar_to_sdl3(mortar_palette), sdl_colors, firstcolor, ncolors);
	int lastcolor = firstcolor + ncolors;
	lastcolor = SDL_min(mortar_palette->ncolors, lastcolor);
	SDL_memcpy(&mortar_palette->colors[firstcolor], mortar_colors, (lastcolor - firstcolor) * sizeof(MORTAR_Color));
	SDL_stack_free(sdl_colors);
	return result;
}

const MORTAR_PixelFormatDetails* MORTAR_GetPixelFormatDetails(MORTAR_PixelFormat format)
{
	ensure_pixelformatdetails_initialized();
	auto it = g_pixelformatdetails.find(format);
	if (it == g_pixelformatdetails.end()) {
		return nullptr;
	}
	return &it->second;
}

uint32_t MORTAR_MapRGBA(
	const MORTAR_PixelFormatDetails* format,
	const MORTAR_Palette* mortar_palette,
	uint8_t r,
	uint8_t g,
	uint8_t b,
	uint8_t a
)
{
	SDL_Palette* sdl3_palette = palette_mortar_to_sdl3(mortar_palette);
	return SDL_MapRGBA((SDL_PixelFormatDetails*) format->reserved, sdl3_palette, r, g, b, a);
}

uint32_t MORTAR_MapRGB(
	const MORTAR_PixelFormatDetails* format,
	const MORTAR_Palette* mortar_palette,
	uint8_t r,
	uint8_t g,
	uint8_t b
)
{
	SDL_Palette* sdl3_palette = mortar_palette ? palette_mortar_to_sdl3(mortar_palette) : NULL;
	return SDL_MapRGB((SDL_PixelFormatDetails*) format->reserved, sdl3_palette, r, g, b);
}

void MORTAR_GetRGBA(
	uint32_t pixelvalue,
	const MORTAR_PixelFormatDetails* format,
	const MORTAR_Palette* mortar_palette,
	uint8_t* r,
	uint8_t* g,
	uint8_t* b,
	uint8_t* a
)
{
	SDL_Palette* sdl3_palette = mortar_palette ? palette_mortar_to_sdl3(mortar_palette) : NULL;
	return SDL_GetRGBA(pixelvalue, (SDL_PixelFormatDetails*) format->reserved, sdl3_palette, r, g, b, a);
}

MORTAR_PixelFormat MORTAR_GetPixelFormatForMasks(
	int bpp,
	uint32_t Rmask,
	uint32_t Gmask,
	uint32_t Bmask,
	uint32_t Amask
)
{
	SDL_PixelFormat sdl3_pixelformat = SDL_GetPixelFormatForMasks(bpp, Rmask, Gmask, Bmask, Amask);
	MORTAR_PixelFormat mortar_pixelformat = pixelformat_sdl3_to_mortar(sdl3_pixelformat);
	SDL_assert(mortar_pixelformat != MORTAR_PIXELFORMAT_UNKNOWN);
	return mortar_pixelformat;
}

const char* MORTAR_GetPixelFormatName(MORTAR_PixelFormat format)
{
	return SDL_GetPixelFormatName(pixelformat_mortar_to_sdl3(format));
}
