#include "config.h"

#include <SDL3/SDL_log.h>
#include <iniparser.h>

void Android_SetupDefaultConfigOverrides(dictionary* p_dictionary)
{
	SDL_Log("Overriding default config for Android");

	iniparser_set(p_dictionary, "isle:diskpath", "/data/data/org.legoisland.Isle.dev/files/DATA/disk/LEGO");
	iniparser_set(p_dictionary, "isle:cdpath", "/data/data/org.legoisland.Isle.dev/files/");
}
