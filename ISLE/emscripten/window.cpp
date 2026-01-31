#include "window.h"

#include "mxtypes.h"

#include <algorithm>
#include <emscripten/html5.h>
#include <mortar/mortar_log.h>

double g_fullWidth;
double g_fullHeight;
bool g_scaleAspect = true;
bool g_originalResolution = true;

extern MxS32 g_targetWidth;
extern MxS32 g_targetHeight;

void Emscripten_SetupWindow(MORTAR_Window* p_window)
{
	MORTAR_SetWindowResizable(p_window, false);

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
			MORTAR_SetWindowSize((MORTAR_Window*) userData, g_targetWidth, g_targetHeight);
		}
		else {
			MORTAR_SetWindowSize((MORTAR_Window*) userData, width, height);
		}

		MORTAR_Log(
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

void Emscripten_ConvertEventToRenderCoordinates(MORTAR_Event* event)
{
	if (!g_scaleAspect) {
		return;
	}

	switch (event->type) {
	case MORTAR_EVENT_MOUSE_MOTION: {
		const float scale = std::min(g_fullWidth / g_targetWidth, g_fullHeight / g_targetHeight);
		const float widthRatio = (g_targetWidth * scale) / g_fullWidth;
		const float heightRatio = (g_targetHeight * scale) / g_fullHeight;
		event->motion.x = (event->motion.x - (g_targetWidth * (1.0f - widthRatio) / 2.0f)) / widthRatio;
		event->motion.x = MORTAR_clamp(event->motion.x, 0, g_targetWidth);
		event->motion.y = (event->motion.y - (g_targetHeight * (1.0f - heightRatio) / 2.0f)) / heightRatio;
		event->motion.y = MORTAR_clamp(event->motion.y, 0, g_targetHeight);
		break;
	}
	case MORTAR_EVENT_MOUSE_BUTTON_DOWN:
	case MORTAR_EVENT_MOUSE_BUTTON_UP: {
		const float scale = std::min(g_fullWidth / g_targetWidth, g_fullHeight / g_targetHeight);
		const float widthRatio = (g_targetWidth * scale) / g_fullWidth;
		const float heightRatio = (g_targetHeight * scale) / g_fullHeight;
		event->button.x = (event->button.x - (g_targetWidth * (1.0f - widthRatio) / 2.0f)) / widthRatio;
		event->button.x = MORTAR_clamp(event->button.x, 0, g_targetWidth);
		event->button.y = (event->button.y - (g_targetHeight * (1.0f - heightRatio) / 2.0f)) / heightRatio;
		event->button.y = MORTAR_clamp(event->button.y, 0, g_targetHeight);
		break;
	}
	case MORTAR_EVENT_FINGER_MOTION:
	case MORTAR_EVENT_FINGER_DOWN:
	case MORTAR_EVENT_FINGER_UP:
	case MORTAR_EVENT_FINGER_CANCELED: {
		const float scale = std::min(g_fullWidth / g_targetWidth, g_fullHeight / g_targetHeight);
		const float widthRatio = (g_targetWidth * scale) / g_fullWidth;
		const float heightRatio = (g_targetHeight * scale) / g_fullHeight;
		event->tfinger.x = (event->tfinger.x * g_targetWidth - (g_targetWidth * (1.0f - widthRatio) / 2.0f)) /
						   widthRatio / g_targetWidth;
		event->tfinger.x = MORTAR_clamp(event->tfinger.x, 0, 1);
		event->tfinger.y = (event->tfinger.y * g_targetHeight - (g_targetHeight * (1.0f - heightRatio) / 2.0f)) /
						   heightRatio / g_targetHeight;
		event->tfinger.y = MORTAR_clamp(event->tfinger.y, 0, 1);
		break;
	}
	}
}
