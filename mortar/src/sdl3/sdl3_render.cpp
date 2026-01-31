#include "mortar/mortar_render.h"
#include "sdl3_internal.h"

MORTAR_Renderer* MORTAR_CreateRenderer(MORTAR_Window* window)
{
	return (MORTAR_Renderer*) SDL_CreateRenderer(window_mortar_to_sdl3(window), nullptr);
}

void MORTAR_DestroyRenderer(MORTAR_Renderer* renderer)
{
	SDL_DestroyRenderer((SDL_Renderer*) renderer);
}

MORTAR_Texture* MORTAR_CreateTexture(MORTAR_Renderer* renderer, MORTAR_PixelFormat format, int w, int h)
{
	return (MORTAR_Texture*) SDL_CreateTexture(
		(SDL_Renderer*) renderer,
		pixelformat_mortar_to_sdl3(format),
		SDL_TEXTUREACCESS_STREAMING,
		w,
		h
	);
}

void MORTAR_DestroyTexture(MORTAR_Texture* texture)
{
	SDL_DestroyTexture((SDL_Texture*) texture);
}

bool MORTAR_RenderTexture(
	MORTAR_Renderer* renderer,
	MORTAR_Texture* texture,
	const MORTAR_FRect* srcrect,
	const MORTAR_FRect* dstrect
)
{
	SDL_FRect sdl3_srcrect_storage;
	SDL_FRect sdl3_dstrect_storage;
	return SDL_RenderTexture(
		(SDL_Renderer*) renderer,
		(SDL_Texture*) texture,
		frect_mortar_to_sdl3(srcrect, &sdl3_srcrect_storage),
		frect_mortar_to_sdl3(dstrect, &sdl3_dstrect_storage)
	);
}

bool MORTAR_RenderPresent(MORTAR_Renderer* renderer)
{
	return SDL_RenderPresent((SDL_Renderer*) renderer);
}

bool MORTAR_UpdateTexture(MORTAR_Texture* texture, const MORTAR_Rect* rect, const void* pixels, int pitch)
{
	SDL_Rect sdl3_rect_storage;
	return SDL_UpdateTexture((SDL_Texture*) texture, rect_mortar_to_sdl3(rect, &sdl3_rect_storage), pixels, pitch);
}
