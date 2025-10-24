#pragma once

#include "SDL.h"
#include "SDL_iostream.h"

#include <filesystem>
#include <vector>

typedef Uint32 SDL_GlobFlags;

typedef enum SDL_PathType
{
	SDL_PATHTYPE_NONE,
	SDL_PATHTYPE_FILE,
	SDL_PATHTYPE_DIRECTORY,
	SDL_PATHTYPE_OTHER
} SDL_PathType;

typedef struct SDL_PathInfo
{
	SDL_PathType type;
	Uint64 size;
} SDL_PathInfo;

// https://github.com/libsdl-org/SDL/blob/main/src/filesystem/

inline char** SDL_GlobDirectory(const char *path, const char *pattern, SDL_GlobFlags flags, int *count)
{
	if (!path || !count) return NULL;
	*count = 0;

	std::vector<std::string> entries;
	try {
		for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
			entries.push_back(std::filesystem::relative(entry.path(), path).string());
		}
	} catch (...) {
		return NULL;
	}
	if (entries.empty()) return NULL;

	char** result = static_cast<char**>(SDL_malloc(sizeof(char*) * entries.size()));
	if (!result) return NULL;

	for (size_t i = 0; i < entries.size(); ++i) {
		result[i] = SDL_strdup(entries[i].c_str());
		if (!result[i]) {
			for (size_t j = 0; j < i; ++j) {
				SDL_free(result[j]);
			}
			SDL_free(result);
			return NULL;
		}
	}
	*count = static_cast<int>(entries.size());
	return result;
}

inline bool SDL_RemovePath(const char *path)
{
	if (!path) return SDL_InvalidParamError("path");
	if (std::filesystem::remove(path)) return true;
	return false;
}
inline bool SDL_RenamePath(const char *oldpath, const char *newpath)
{
	if (!oldpath) return SDL_InvalidParamError("oldpath");
	if (!newpath) return SDL_InvalidParamError("newpath");

	std::filesystem::rename(oldpath, newpath);
	return true;
}

inline bool SDL_GetPathInfo(const char *path, SDL_PathInfo *info)
{
	if (!path) return SDL_InvalidParamError("path");

	SDL_PathInfo dummy;
	if (!info)  info = &dummy;

	SDL_zerop(info);
	switch (const auto status = std::filesystem::status(path);status.type()) {
	case std::filesystem::file_type::regular:
		info->type = SDL_PATHTYPE_FILE;
		info->size = std::filesystem::file_size(path);
		break;
	case std::filesystem::file_type::directory:
		info->type = SDL_PATHTYPE_DIRECTORY;
		info->size = 0;
		break;
	case std::filesystem::file_type::not_found:
		info->type = SDL_PATHTYPE_NONE;
		info->size = 0;
		return false;
	default:
		info->type = SDL_PATHTYPE_OTHER;
		info->size = 0;
		break;
	}
	return true;
}
