#include "config.h"

#include <SDL3/SDL_log.h>
#include <iniparser.h>

void VITA_SetupDefaultConfigOverrides(dictionary* p_dictionary)
{
	SDL_Log("Overriding default config for VITA");

	iniparser_set(p_dictionary, "isle:diskpath", "app0:");
	iniparser_set(p_dictionary, "isle:cdpath", "app0:cd/");
	iniparser_set(p_dictionary, "isle:UseJoystick", "true");
	iniparser_set(p_dictionary, "isle:Draw Cursor", "true");

	// Use e_noAnimation/cut transition
	//iniparser_set(p_dictionary, "isle:Transition Type", "1");
}
