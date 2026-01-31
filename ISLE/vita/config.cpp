#include "config.h"

#include <iniparser.h>
#include <mortar/mortar_log.h>

void VITA_SetupDefaultConfigOverrides(dictionary* p_dictionary)
{
	MORTAR_Log("Overriding default config for VITA");

	iniparser_set(p_dictionary, "isle:diskpath", "ux0:data/isledecomp/isle/disk");
	iniparser_set(p_dictionary, "isle:cdpath", "ux0:data/isledecomp/isle/cd");
	iniparser_set(p_dictionary, "isle:MSAA", "4");
}
