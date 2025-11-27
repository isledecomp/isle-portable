#include "config.h"

#include <iniparser.h>
#include <mortar/mortar_log.h>

void NX_SetupDefaultConfigOverrides(dictionary* p_dictionary)
{
	iniparser_set(p_dictionary, "isle:diskpath", "sdmc:/switch/isle/LEGO/");
	iniparser_set(p_dictionary, "isle:cdpath", "sdmc:/switch/isle/");
	iniparser_set(p_dictionary, "isle:savepath", "sdmc:/switch/isle/");
	iniparser_set(p_dictionary, "isle:Cursor Sensitivity", "16.000000");
}
