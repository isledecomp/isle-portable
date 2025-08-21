#include "config.h"

#include "mxstring.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_system.h>
#include <iniparser.h>

void Android_SetupDefaultConfigOverrides(dictionary* p_dictionary)
{
	SDL_Log("Overriding default config for Android");

	const char* data = SDL_GetAndroidExternalStoragePath();
	MxString savedata = MxString(data) + "/saves/";

	if (!SDL_GetPathInfo(savedata.GetData(), NULL)) {
		SDL_CreateDirectory(savedata.GetData());
	}

	iniparser_set(p_dictionary, "isle:diskpath", data);
	iniparser_set(p_dictionary, "isle:cdpath", data);
	iniparser_set(p_dictionary, "isle:savepath", savedata.GetData());

	// Default to Virtual Mouse
	char buf[16];
	iniparser_set(p_dictionary, "isle:Touch Scheme", SDL_itoa(0, buf, 10));
}
