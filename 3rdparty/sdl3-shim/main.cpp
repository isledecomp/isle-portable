#include <SDL2/SDL.h>
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_main.h"

static void TranslateSDLEvents(SDL_Event* e)
{
	// Extend to SDL_DISPLAYEVENT & fully replace the event object passed to AppEvent
	// if wanting to drop the ifs on key, mouse and keyboard events.
	if (e->type == SDL_WINDOWEVENT) {
		e->type = SDL_WINDOWEVENT + 2 + e->window.event;
	}
}

int main(int argc, char *argv[]) {
	void *appstate = NULL;
	if (SDL_AppInit(&appstate, argc, argv) != 0) {
		return 1;
	}

	SDL_Event e;
	while (!SDL_AppIterate(appstate)) {
		while (SDL_PollEvent(&e)) {
			TranslateSDLEvents(&e);
			SDL_AppEvent(appstate, &e);
		}
	}

	SDL_AppQuit(appstate, static_cast<SDL_AppResult>(NULL));
	return 0;
}
