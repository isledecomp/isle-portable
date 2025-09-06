#pragma once

#include "SDL.h"
#include "SDL_iostream.h"

// TODO: properly implement (

typedef Uint32 SDL_GlobFlags;

typedef enum SDL_PathType
{
	SDL_PATHTYPE_NONE,
	SDL_PATHTYPE_FILE,
	SDL_PATHTYPE_DIRECTORY,
	SDL_PATHTYPE_OTHER
} SDL_PathType;

typedef Sint64 SDL_Time;

typedef struct SDL_PathInfo
{
	SDL_PathType type;
	Uint64 size;
	SDL_Time create_time;
	SDL_Time modify_time;
	SDL_Time access_time;
} SDL_PathInfo;

// https://github.com/libsdl-org/SDL/blob/main/src/filesystem/

inline char** SDL_GlobDirectory(const char *path, const char *pattern, SDL_GlobFlags flags, int *count)
{
	// since the one use of this doesnt use pattern or flags this should be a pretty simple stub
	SDL_Unsupported();
	return NULL;
}

inline bool SDL_RemovePath(const char *path)
{
	return SDL_Unsupported();
}
inline bool SDL_RenamePath(const char *oldpath, const char *newpath)
{
	return SDL_Unsupported();
}

inline bool SDL_GetPathInfo(const char *path, SDL_PathInfo *info)
{
	return SDL_Unsupported();
}
