#pragma once
#include <SDL3/SDL.h>

class Lego1App {
public:
	Lego1App() {}

	~Lego1App() {}

	void Run()
	{
		SDL_Log("Lego1App::Run called");

		bool running = true;
		SDL_Event e;

		while (running) {
			while (SDL_PollEvent(&e)) {
				if (e.type == SDL_QUIT) {
					running = false;
				}
			}
		}
	}
};
