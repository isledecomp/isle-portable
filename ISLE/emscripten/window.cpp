#include "window.h"

#include "mxtypes.h"

#include <SDL3/SDL_log.h>
#include <algorithm>
#include <emscripten/html5.h>

double g_fullWidth;
double g_fullHeight;
bool g_scaleAspect = true;
bool g_originalResolution = true;

extern MxS32 g_targetWidth;
extern MxS32 g_targetHeight;

void Emscripten_SetupWindow(SDL_Window* p_window)
{
	EmscriptenFullscreenStrategy strategy;
	strategy.scaleMode = g_scaleAspect ? EMSCRIPTEN_FULLSCREEN_SCALE_ASPECT : EMSCRIPTEN_FULLSCREEN_SCALE_STRETCH;
	strategy.canvasResolutionScaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_HIDEF;
	strategy.filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT;
	strategy.canvasResizedCallbackUserData = p_window;
	strategy.canvasResizedCallback = [](int eventType, const void* reserved, void* userData) -> bool {
		int width, height;
		emscripten_get_canvas_element_size("canvas", &width, &height);
		emscripten_get_element_css_size("#canvas", &g_fullWidth, &g_fullHeight);

		if (g_originalResolution) {
			SDL_SetWindowSize((SDL_Window*) userData, g_targetWidth, g_targetHeight);
		}
		else {
			SDL_SetWindowSize((SDL_Window*) userData, width, height);
		}

		SDL_Log(
			"Emscripten: window size %dx%d, canvas size %dx%d, scale aspect %s, original resolution %s",
			width,
			height,
			(int) g_fullWidth,
			(int) g_fullHeight,
			g_scaleAspect ? "TRUE" : "FALSE",
			g_originalResolution ? "TRUE" : "FALSE"
		);
		return true;
	};

	emscripten_enter_soft_fullscreen("canvas", &strategy);
}

void Emscripten_SetScaleAspect(bool p_scaleAspect)
{
	g_scaleAspect = p_scaleAspect;
}

void Emscripten_SetOriginalResolution(bool p_originalResolution)
{
	g_originalResolution = p_originalResolution;
}

void Emscripten_ConvertEventToRenderCoordinates(SDL_Event* event)
{
	if (!g_scaleAspect) {
		return;
	}

	switch (event->type) {
	case SDL_EVENT_MOUSE_MOTION: {
		const float scale = std::min(g_fullWidth / g_targetWidth, g_fullHeight / g_targetHeight);
		const float widthRatio = (g_targetWidth * scale) / g_fullWidth;
		const float heightRatio = (g_targetHeight * scale) / g_fullHeight;
		event->motion.x = (event->motion.x - (g_targetWidth * (1.0f - widthRatio) / 2.0f)) / widthRatio;
		event->motion.x = SDL_clamp(event->motion.x, 0, g_targetWidth);
		event->motion.y = (event->motion.y - (g_targetHeight * (1.0f - heightRatio) / 2.0f)) / heightRatio;
		event->motion.y = SDL_clamp(event->motion.y, 0, g_targetHeight);
		break;
	}
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP: {
		const float scale = std::min(g_fullWidth / g_targetWidth, g_fullHeight / g_targetHeight);
		const float widthRatio = (g_targetWidth * scale) / g_fullWidth;
		const float heightRatio = (g_targetHeight * scale) / g_fullHeight;
		event->button.x = (event->button.x - (g_targetWidth * (1.0f - widthRatio) / 2.0f)) / widthRatio;
		event->button.x = SDL_clamp(event->button.x, 0, g_targetWidth);
		event->button.y = (event->button.y - (g_targetHeight * (1.0f - heightRatio) / 2.0f)) / heightRatio;
		event->button.y = SDL_clamp(event->button.y, 0, g_targetHeight);
		break;
	}
	case SDL_EVENT_FINGER_MOTION:
	case SDL_EVENT_FINGER_DOWN:
	case SDL_EVENT_FINGER_UP:
	case SDL_EVENT_FINGER_CANCELED: {
		const float scale = std::min(g_fullWidth / g_targetWidth, g_fullHeight / g_targetHeight);
		const float widthRatio = (g_targetWidth * scale) / g_fullWidth;
		const float heightRatio = (g_targetHeight * scale) / g_fullHeight;
		event->tfinger.x = (event->tfinger.x * g_targetWidth - (g_targetWidth * (1.0f - widthRatio) / 2.0f)) /
						   widthRatio / g_targetWidth;
		event->tfinger.x = SDL_clamp(event->tfinger.x, 0, 1);
		event->tfinger.y = (event->tfinger.y * g_targetHeight - (g_targetHeight * (1.0f - heightRatio) / 2.0f)) /
						   heightRatio / g_targetHeight;
		event->tfinger.y = SDL_clamp(event->tfinger.y, 0, 1);
		break;
	}
	}
}
