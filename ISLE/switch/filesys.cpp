#include <cstdio>
#include <sys/stat.h>

#include "filesys.h"

bool NX_GetPathInfo(const char *path, SDL_PathInfo *info)
{
    struct stat st_info;
    if (stat(path, &st_info) != 0)
    {
        if (info) info->type = SDL_PATHTYPE_NONE;
        return false;
    }

    if (st_info.st_mode & S_IFDIR)
    {
        if (info)info->type = SDL_PATHTYPE_DIRECTORY;
        return true;
    }

    if (info) info->type = SDL_PATHTYPE_FILE;
    if (!info) return true;

    auto *fp = fopen(path, "r");
    fseek(fp, 0, SEEK_END);
    if (info) info->size = ftell(fp);
    fclose(fp);

    return true;
}