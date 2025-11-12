#include <cstdio>
#include <sys/stat.h>
#include <dirent.h>
#include <cerrno>
#include <SDL3/SDL.h>
#include "filesys.h"


// Missing from Switch SDL3 implementation
#ifndef S_ISREG
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif

// Uses core of SDL's SDL_GetPathInfo but handles null 'info'.
bool NX_GetPathInfo(const char *path, SDL_PathInfo *info)
{
    SDL_PathInfo tmp_info;
    struct stat statbuf;
    const int rc = stat(path, &statbuf);

    if (rc < 0) {
        return SDL_SetError("Can't stat: %s", strerror(errno));
    } else if (S_ISREG(statbuf.st_mode)) {
        tmp_info.type = SDL_PATHTYPE_FILE;
        tmp_info.size = (Uint64) statbuf.st_size;
    } else if (S_ISDIR(statbuf.st_mode)) {
        tmp_info.type = SDL_PATHTYPE_DIRECTORY;
        tmp_info.size = 0;
    } else {
        tmp_info.type = SDL_PATHTYPE_OTHER;
        tmp_info.size = (Uint64) statbuf.st_size;
    }

    tmp_info.create_time = (SDL_Time)SDL_SECONDS_TO_NS(statbuf.st_ctime);
    tmp_info.modify_time = (SDL_Time)SDL_SECONDS_TO_NS(statbuf.st_mtime);
    tmp_info.access_time = (SDL_Time)SDL_SECONDS_TO_NS(statbuf.st_atime);

    if (info) *info = tmp_info;

    return true;
}