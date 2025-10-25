#include "config.h"

#include <SDL3/SDL_log.h>
#include <iniparser.h>

// copy of 3ds config

void WIIU_SetupDefaultConfigOverrides(dictionary* p_dictionary)
{
	SDL_Log("Overriding default config for Wii U");

	iniparser_set(p_dictionary, "isle:diskpath", "sdmc:/wiiu/apps/isle-U/content/LEGO");
	iniparser_set(p_dictionary, "isle:cdpath", "sdmc:/wiiu/apps/isle-U/content/LEGO");
}
