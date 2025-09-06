#include <SDL2/SDL.h>
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_main.h"

int main(int argc, char *argv[]) {
	void *appstate = NULL;
	if (SDL_AppInit(&appstate, argc, argv) != 0) {
		return 1;
	}

	SDL_Event e;
	while (!SDL_AppIterate(appstate)) {
		while (SDL_PollEvent(&e)) {
			SDL_AppEvent(appstate, &e);
		}
	}

	SDL_AppQuit(appstate, static_cast<SDL_AppResult>(NULL));
	return 0;
}
