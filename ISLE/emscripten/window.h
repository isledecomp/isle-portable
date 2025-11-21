#ifndef EMSCRIPTEN_WINDOW_H
#define EMSCRIPTEN_WINDOW_H

#include <mortar/mortar.h>

void Emscripten_SetupWindow(MORTAR_Window* p_window);
void Emscripten_SetScaleAspect(bool p_scaleAspect);
void Emscripten_SetOriginalResolution(bool p_originalResolution);
void Emscripten_ConvertEventToRenderCoordinates(MORTAR_Event* event);

#endif // EMSCRIPTEN_WINDOW_H
