#ifndef EMSCRIPTEN_MESSAGE_BOX_H
#define EMSCRIPTEN_MESSAGE_BOX_H

#include <SDL3/SDL_messagebox.h>

bool Emscripten_ShowSimpleMessageBox(
	SDL_MessageBoxFlags flags,
	const char* title,
	const char* message,
	SDL_Window* window
);

#endif // EMSCRIPTEN_MESSAGE_BOX_H
