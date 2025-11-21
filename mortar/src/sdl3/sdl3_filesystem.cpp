#include "mortar/mortar_filesystem.h"
#include "sdl3_internal.h"

#include <stdlib.h>

static MORTAR_PathType pathtype_sdl3_to_mortar(SDL_PathType path_type)
{
	switch (path_type) {
	case SDL_PATHTYPE_NONE:
		return MORTAR_PATHTYPE_NONE;
	case SDL_PATHTYPE_FILE:
		return MORTAR_PATHTYPE_FILE;
	case SDL_PATHTYPE_DIRECTORY:
		return MORTAR_PATHTYPE_DIRECTORY;
	case SDL_PATHTYPE_OTHER:
		return MORTAR_PATHTYPE_OTHER;
	default:
		abort();
	}
}

static SDL_Folder folder_mortar_to_sdl3(MORTAR_Folder mortar_folder)
{
	switch (mortar_folder) {
	case MORTAR_FOLDER_DOCUMENTS:
		return SDL_FOLDER_DOCUMENTS;
	default:
		abort();
	}
}

char** MORTAR_GlobDirectory(const char* path, const char* pattern, int* count)
{
	return SDL_GlobDirectory(path, pattern, 0, count);
}

bool MORTAR_GetPathInfo(const char* path, MORTAR_PathInfo* info)
{
	SDL_PathInfo sdl3_info;
	bool result = SDL_GetPathInfo(path, &sdl3_info);
	if (result && info) {
		info->type = pathtype_sdl3_to_mortar(sdl3_info.type);
		info->size = sdl3_info.size;
		info->create_time = sdl3_info.create_time;
		info->modify_time = sdl3_info.modify_time;
		info->access_time = sdl3_info.access_time;
	}
	return result;
}

bool MORTAR_RemovePath(const char* path)
{
	return SDL_RemovePath(path);
}

bool MORTAR_RenamePath(const char* oldpath, const char* newpath)
{
	return SDL_RenamePath(oldpath, newpath);
}

const char* MORTAR_GetBasePath()
{
	return SDL_GetBasePath();
}

char* MORTAR_GetPrefPath(const char* org, const char* app)
{
	return SDL_GetPrefPath(org, app);
}

const char* MORTAR_GetUserFolder(MORTAR_Folder folder)
{
	return SDL_GetUserFolder(folder_mortar_to_sdl3(folder));
}

bool MORTAR_CreateDirectory(const char* path)
{
	return SDL_CreateDirectory(path);
}
