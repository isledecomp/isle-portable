#ifndef EMSCRIPTEN_WINDOW_H
#define EMSCRIPTEN_WINDOW_H

#include <SDL3/SDL.h>

void Emscripten_SetupWindow(SDL_Window* p_window);
void Emscripten_SetScaleAspect(bool p_scaleAspect);
void Emscripten_SetOriginalResolution(bool p_originalResolution);
void Emscripten_ConvertEventToRenderCoordinates(SDL_Event* event);

#endif // EMSCRIPTEN_WINDOW_H
