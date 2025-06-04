#include "messagebox.h"

#include <emscripten.h>

bool Emscripten_ShowSimpleMessageBox(
	SDL_MessageBoxFlags flags,
	const char* title,
	const char* message,
	SDL_Window* window
)
{
	MAIN_THREAD_EM_ASM({alert(UTF8ToString($0) + "\n\n" + UTF8ToString($1))}, title, message);
	return true;
}
