#ifndef VITA_MESSAGE_BOX_H
#define VITA_MESSAGE_BOX_H

#include <mortar/mortar_messagebox.h>

bool Vita_ShowSimpleMessageBox(
	MORTAR_MessageBoxFlags flags,
	const char* title,
	const char* message,
	MORTAR_Window* window
);

#endif // VITA_MESSAGE_BOX_H
