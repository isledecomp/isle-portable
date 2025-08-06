#include "config.h"

#ifndef NO_SDL3
#include <SDL3/SDL_Log.h>
#endif

#include <iniparser.h>

void WIIU_SetupDefaultConfigOverrides(dictionary* p_dictionary)
{
#ifndef NO_SDL3
    SDL_Log("Overriding default config for wiiu");
#endif

    iniparser_set(p_dictionary, "isle:diskpath", "sdmc:/wiiu/apps/isle-u/content/ISLE/LEGO/disk");
    iniparser_set(p_dictionary, "isle:cdpath", "sdmc:/wiiu/apps/isle-u/content/ISLE/");
    iniparser_set(p_dictionary, "isle:savepath", "sdmc:/wiiu/apps/isle-u/content/ISLE/SAVE");
}
