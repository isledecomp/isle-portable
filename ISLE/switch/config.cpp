#include "config.h"

#include <SDL3/SDL_log.h>
#include <iniparser.h>

void NX_SetupDefaultConfigOverrides(dictionary* p_dictionary)
{
	iniparser_set(p_dictionary, "isle:diskpath", "sdmc:/switch/isle/LEGO/");
	iniparser_set(p_dictionary, "isle:cdpath", "sdmc:/switch/isle/");
	iniparser_set(p_dictionary, "isle:savepath", "sdmc:/switch/isle/");
}
