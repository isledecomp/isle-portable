#ifndef IOS_FILEPICKER_H
#define IOS_FILEPICKER_H

#include <SDL3/SDL_video.h>

// Asks the user to select the folder containing the game files and imports them
// into the app's Documents storage at p_destRoot (the configured diskpath).
bool IOS_TryImportGameFiles(SDL_Window* p_window, const char* p_destRoot);

#endif // IOS_FILEPICKER_H
