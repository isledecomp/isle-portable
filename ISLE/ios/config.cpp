#include "config.h"

#include "mxstring.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_log.h>
#include <iniparser.h>

void IOS_SetupDefaultConfigOverrides(dictionary* p_dictionary)
{
	SDL_Log("Overriding default config for iOS");

	// Use DevelopmentFiles path for disk and cd paths
	// It's good to use that path since user can easily
	// connect through SMB and copy the files
	MxString documentFolder = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
	documentFolder += "isle";

	if (!SDL_GetPathInfo(documentFolder.GetData(), NULL)) {
		SDL_CreateDirectory(documentFolder.GetData());
	}

	iniparser_set(p_dictionary, "isle:diskpath", documentFolder.GetData());
	iniparser_set(p_dictionary, "isle:cdpath", documentFolder.GetData());
}
