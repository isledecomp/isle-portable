#include "config.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_log.h>
#include <iniparser.h>

void IOS_SetupDefaultConfigOverrides(dictionary* p_dictionary)
{
	SDL_Log("Overriding default config for iOS");

	// Use DevelopmentFiles path for disk and cd paths
	// It's good to use that path since user can easily
	// connect through SMB and copy the files
	const char* documentFolder = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
	char* diskPath = new char[strlen(documentFolder) + strlen("isle") + 1]();
	strcpy(diskPath, documentFolder);
	strcat(diskPath, "isle");

	if (!SDL_GetPathInfo(diskPath, NULL)) {
		SDL_CreateDirectory(diskPath);
	}

	iniparser_set(p_dictionary, "isle:diskpath", diskPath);
	iniparser_set(p_dictionary, "isle:cdpath", diskPath);
}
