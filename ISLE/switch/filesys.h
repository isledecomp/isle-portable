#ifndef NX_FILESYS_H
#define NX_FILESYS_H

#include <SDL3/SDL_filesystem.h>

#define SDL_GetPathInfo NX_GetPathInfo // Override broken SDL_GetPathInfo
bool NX_GetPathInfo(const char* path, SDL_PathInfo* info);

#endif // NX_FILESYS_H
