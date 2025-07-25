#pragma once

#include "extensions/extensions.h"
#include "legotextureinfo.h"

#include <array>
#include <map>
#include <vector>

namespace Extensions
{
class TextureLoader {
public:
	static void Initialize();
	static bool PatchTexture(LegoTextureInfo* p_textureInfo);

	static std::map<std::string, std::string> options;
	static std::vector<std::string> excludedFiles;
	static bool enabled;

	static constexpr std::array<std::pair<std::string_view, std::string_view>, 1> defaults = {
		{{"texture loader:texture path", "/textures"}}
	};

private:
	static SDL_Surface* FindTexture(const char* p_name);
};

#ifdef EXTENSIONS
constexpr auto PatchTexture = &TextureLoader::PatchTexture;
#else
constexpr decltype(&TextureLoader::PatchTexture) PatchTexture = nullptr;
#endif
}; // namespace Extensions
