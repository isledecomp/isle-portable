#pragma once

#include "mortar_begin.h"

#include <stdint.h>

typedef uint32_t MORTAR_GlobFlags;

typedef int64_t MORTAR_Time;

typedef enum MORTAR_Folder {
	MORTAR_FOLDER_DOCUMENTS
} MORTAR_Folder;

typedef enum MORTAR_PathType {
	MORTAR_PATHTYPE_NONE,
	MORTAR_PATHTYPE_FILE,
	MORTAR_PATHTYPE_DIRECTORY,
	MORTAR_PATHTYPE_OTHER
} MORTAR_PathType;

typedef struct MORTAR_PathInfo {
	MORTAR_PathType type;
	uint64_t size;
	MORTAR_Time create_time;
	MORTAR_Time modify_time;
	MORTAR_Time access_time;
} MORTAR_PathInfo;

extern MORTAR_DECLSPEC char** MORTAR_GlobDirectory(const char* path, const char* pattern, int* count);

extern MORTAR_DECLSPEC bool MORTAR_GetPathInfo(const char* path, MORTAR_PathInfo* info);

extern MORTAR_DECLSPEC bool MORTAR_RemovePath(const char* path);

extern MORTAR_DECLSPEC bool MORTAR_RenamePath(const char* oldpath, const char* newpath);

extern MORTAR_DECLSPEC const char* MORTAR_GetBasePath(void);

extern MORTAR_DECLSPEC char* MORTAR_GetPrefPath(const char* org, const char* app);

extern MORTAR_DECLSPEC const char* MORTAR_GetUserFolder(MORTAR_Folder folder);

extern MORTAR_DECLSPEC bool MORTAR_CreateDirectory(const char* path);

#include "mortar_end.h"
