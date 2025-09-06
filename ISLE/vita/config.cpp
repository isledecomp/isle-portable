#include "config.h"

#include <SDL3/SDL_log.h>
#include <iniparser.h>

void VITA_SetupDefaultConfigOverrides(dictionary* p_dictionary)
{
	SDL_Log("Overriding default config for VITA");

	iniparser_set(p_dictionary, "isle:diskpath", "ux0:data/isledecomp/DATA/disk");
	iniparser_set(p_dictionary, "isle:cdpath", "ux0:data/isledecomp/");
	iniparser_set(p_dictionary, "isle:MSAA", "4");
}
