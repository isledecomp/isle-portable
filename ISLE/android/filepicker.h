#ifndef ANDROID_FILEPICKER_H
#define ANDROID_FILEPICKER_H

#include <SDL3/SDL_video.h>

// Asks the user to select the folder containing the game files and imports them
// into the app's storage. On success, *p_hdPath is updated to the directory the
// files were imported into (freeing the previous value) and the new diskpath is
// persisted to the config at p_iniPath (or the default location if NULL).
bool Android_TryImportGameFiles(SDL_Window* p_window, const char* p_iniPath, char** p_hdPath);

#endif // ANDROID_FILEPICKER_H
