#include "config.h"

#include <SDL3/SDL_log.h>
#include <iniparser.h>

void XBONE_SetupDefaultConfigOverrides(dictionary* p_dictionary)
{
	SDL_Log("Overriding default config for Xbox One/Series");

	// Use DevelopmentFiles path for disk and cd paths
	// It's good to use that path since user can easily
	// connect through SMB and copy the files
	iniparser_set(p_dictionary, "isle:diskpath", "D:\\DevelopmentFiles\\isle\\");
	iniparser_set(p_dictionary, "isle:cdpath", "D:\\DevelopmentFiles\\isle\\");

	// Enable cursor by default
	iniparser_set(p_dictionary, "isle:Draw Cursor", "true");
}
