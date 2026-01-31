#include "mortar/mortar_messagebox.h"
#include "sdl3_internal.h"

SDL_MessageBoxFlags messageboxflags_mortar_to_sdl3(MORTAR_MessageBoxFlags flags)
{
	SDL_MessageBoxFlags result = 0;

	if (flags & MORTAR_MESSAGEBOX_ERROR) {
		result |= SDL_MESSAGEBOX_ERROR;
	}
	if (flags & MORTAR_MESSAGEBOX_WARNING) {
		result |= SDL_MESSAGEBOX_WARNING;
	}
	if (flags & MORTAR_MESSAGEBOX_INFORMATION) {
		result |= SDL_MESSAGEBOX_INFORMATION;
	}
	if (flags & MORTAR_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT) {
		result |= SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT;
	}
	if (flags & MORTAR_MESSAGEBOX_BUTTONS_RIGHT_TO_LEFT) {
		result |= SDL_MESSAGEBOX_BUTTONS_RIGHT_TO_LEFT;
	}
	return result;
}

bool MORTAR_ShowSimpleMessageBox(
	MORTAR_MessageBoxFlags flags,
	const char* title,
	const char* message,
	MORTAR_Window* window
)
{
	return SDL_ShowSimpleMessageBox(
		messageboxflags_mortar_to_sdl3(flags),
		title,
		message,
		window_mortar_to_sdl3(window)
	);
}
