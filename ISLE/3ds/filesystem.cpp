#include "filesystem.h"

#include <SDL3/SDL_log.h>
#include <iniparser.h>

void N3DS_SetupDefaultConfigOverrides(dictionary* p_dictionary)
{
	SDL_Log("Overriding default config for 3DS");

	// We are currently not bundling the assets into romfs.
	// User must place assets in sdmc:/3ds/isle where
	// sdmc:/3ds/isle/LEGO/SCRIPTS/CREDITS.si exists, for example.
	iniparser_set(p_dictionary, "isle:diskpath", "sdmc:/3ds/isle/LEGO/disk");
	iniparser_set(p_dictionary, "isle:cdpath", "sdmc:/3ds/isle");

	// TODO: Save path: can we use libctru FS save data functions? Would be neat, especially for CIA install
	// Extra / at the end causes some issues
	iniparser_set(p_dictionary, "isle:savepath", "sdmc:/3ds/isle");

	// We are currently just rendering to the touch screen
	iniparser_set(p_dictionary, "isle:Full Screen", "true");

	// Wide view angle takes more resources
	iniparser_set(p_dictionary, "isle:Wide View Angle", "false");

	// Set back buffers in video RAM
	iniparser_set(p_dictionary, "isle:Back Buffers in Video RAM", "1");

	// Use e_noAnimation/cut transition
	iniparser_set(p_dictionary, "isle:Transition Type", "1");
}
