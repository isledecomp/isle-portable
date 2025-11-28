#ifndef EMSCRIPTEN_MESSAGE_BOX_H
#define EMSCRIPTEN_MESSAGE_BOX_H

#include <mortar/mortar_messagebox.h>

bool Emscripten_ShowSimpleMessageBox(
	MORTAR_MessageBoxFlags flags,
	const char* title,
	const char* message,
	MORTAR_Window* window
);

#endif // EMSCRIPTEN_MESSAGE_BOX_H
