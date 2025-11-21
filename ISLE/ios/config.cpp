#include "config.h"

#include "mxstring.h"

#include <iniparser.h>
#include <mortar/mortar_filesystem.h>
#include <mortar/mortar_log.h>

void IOS_SetupDefaultConfigOverrides(dictionary* p_dictionary)
{
	MORTAR_Log("Overriding default config for iOS");

	// Use DevelopmentFiles path for disk and cd paths
	// It's good to use that path since user can easily
	// connect through SMB and copy the files
	MxString documentFolder = MORTAR_GetUserFolder(MORTAR_FOLDER_DOCUMENTS);
	documentFolder += "isle";

	if (!MORTAR_GetPathInfo(documentFolder.GetData(), NULL)) {
		MORTAR_CreateDirectory(documentFolder.GetData());
	}

	iniparser_set(p_dictionary, "isle:diskpath", documentFolder.GetData());
	iniparser_set(p_dictionary, "isle:cdpath", documentFolder.GetData());
}
