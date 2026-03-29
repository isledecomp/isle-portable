#pragma once

#include "extensions/extensions.h"
#include "legotextureinfo.h"

#include <array>
#include <map>
#include <vector>

namespace Extensions
{
class TextureLoaderExt {
public:
	static void Initialize();
	static bool PatchTexture(LegoTextureInfo* p_textureInfo);
	static void AddExcludedFile(const std::string& p_file);

	static std::map<std::string, std::string> options;
	static bool enabled;

	static constexpr std::array<std::pair<std::string_view, std::string_view>, 1> defaults = {
		{{"texture loader:texture path", "/textures"}}
	};

private:
	static std::vector<std::string> excludedFiles;
	static SDL_Surface* FindTexture(const char* p_name);
};

namespace TL
{
#ifdef EXTENSIONS
constexpr auto PatchTexture = &TextureLoaderExt::PatchTexture;
#else
constexpr decltype(&TextureLoaderExt::PatchTexture) PatchTexture = nullptr;
#endif
} // namespace TL

}; // namespace Extensions
