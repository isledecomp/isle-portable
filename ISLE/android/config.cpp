#include "config.h"

#include "mxstring.h"

#include <iniparser.h>
#include <mortar/mortar.h>

void Android_SetupDefaultConfigOverrides(dictionary* p_dictionary)
{
	MORTAR_Log("Overriding default config for Android");

	const char* data = MORTAR_GetAndroidExternalStoragePath();
	MxString savedata = MxString(data) + "/saves/";

	if (!MORTAR_GetPathInfo(savedata.GetData(), NULL)) {
		MORTAR_CreateDirectory(savedata.GetData());
	}

	iniparser_set(p_dictionary, "isle:diskpath", data);
	iniparser_set(p_dictionary, "isle:cdpath", data);
	iniparser_set(p_dictionary, "isle:mediapath", data);
	iniparser_set(p_dictionary, "isle:savepath", savedata.GetData());

	// Default to Virtual Mouse
	char buf[16];
	iniparser_set(p_dictionary, "isle:Touch Scheme", MORTAR_itoa(0, buf, 10));
}

extern int main(int argc, char* argv[]);

extern "C"
{

	int SDL_main(int argc, char* argv[])
	{
		return main(argc, argv);
	}
}
