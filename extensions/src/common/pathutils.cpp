#include "extensions/common/pathutils.h"

#include "legomain.h"

#include <SDL3/SDL_filesystem.h>

using namespace Extensions::Common;

bool Extensions::Common::ResolveGamePath(const char* p_relativePath, MxString& p_outPath)
{
	p_outPath = MxString(MxOmni::GetHD()) + p_relativePath;
	p_outPath.MapPathToFilesystem();
	if (SDL_GetPathInfo(p_outPath.GetData(), nullptr)) {
		return true;
	}

	p_outPath = MxString(MxOmni::GetCD()) + p_relativePath;
	p_outPath.MapPathToFilesystem();
	if (SDL_GetPathInfo(p_outPath.GetData(), nullptr)) {
		return true;
	}

	return false;
}
