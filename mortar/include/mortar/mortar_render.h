#pragma once

#include "mortar/mortar_pixels.h"
#include "mortar/mortar_rect.h"
#include "mortar/mortar_video.h"
#include "mortar_begin.h"

typedef struct MORTAR_Renderer MORTAR_Renderer;
typedef struct MORTAR_Texture MORTAR_Texture;

extern MORTAR_DECLSPEC MORTAR_Renderer* MORTAR_CreateRenderer(MORTAR_Window* window);

extern MORTAR_DECLSPEC void MORTAR_DestroyRenderer(MORTAR_Renderer* renderer);

extern MORTAR_DECLSPEC MORTAR_Texture* MORTAR_CreateTexture(
	MORTAR_Renderer* renderer,
	MORTAR_PixelFormat format,
	int w,
	int h
);

extern MORTAR_DECLSPEC void MORTAR_DestroyTexture(MORTAR_Texture* texture);

extern MORTAR_DECLSPEC bool MORTAR_RenderTexture(
	MORTAR_Renderer* renderer,
	MORTAR_Texture* texture,
	const MORTAR_FRect* srcrect,
	const MORTAR_FRect* dstrect
);

extern MORTAR_DECLSPEC bool MORTAR_RenderPresent(MORTAR_Renderer* renderer);

extern MORTAR_DECLSPEC bool MORTAR_UpdateTexture(
	MORTAR_Texture* texture,
	const MORTAR_Rect* rect,
	const void* pixels,
	int pitch
);

#include "mortar_end.h"
