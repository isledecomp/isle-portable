#ifndef VITA_MESSAGE_BOX_H
#define VITA_MESSAGE_BOX_H

#include <SDL3/SDL_messagebox.h>

bool Vita_ShowSimpleMessageBox(SDL_MessageBoxFlags flags, const char* title, const char* message, SDL_Window* window);

#endif // VITA_MESSAGE_BOX_H
