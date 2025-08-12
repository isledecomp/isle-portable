#include "config.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_system.h>
#include <iniparser.h>

void Android_SetupDefaultConfigOverrides(dictionary* p_dictionary)
{
	SDL_Log("Overriding default config for Android");

	const char* data = SDL_GetAndroidExternalStoragePath();
	char* savedata = new char[strlen(data) + strlen("/saves/") + 1];
	strcpy(savedata, data);
	strcat(savedata, "/saves/");

	if (!SDL_GetPathInfo(savedata, NULL)) {
		SDL_CreateDirectory(savedata);
	}

	iniparser_set(p_dictionary, "isle:diskpath", data);
	iniparser_set(p_dictionary, "isle:cdpath", data);
	iniparser_set(p_dictionary, "isle:mediapath", data);

	iniparser_set(p_dictionary, "isle:savepath", savedata);

	// Default to Virtal Mouse
	char buf[16];
	iniparser_set(p_dictionary, "isle:Touch Scheme", SDL_itoa(0, buf, 10));
}
