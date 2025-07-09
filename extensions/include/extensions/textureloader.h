#pragma once

#include "extensions/extensions.h"
#include "legotextureinfo.h"

namespace Extensions
{
class TextureLoader {
public:
	static constexpr const char* key = "extensions:texture loader";

	static bool PatchTexture(LegoTextureInfo* p_textureInfo);

private:
	static constexpr const char* texturePath = "/textures/";

	static SDL_Surface* FindTexture(const char* p_name);
};

#ifdef EXTENSIONS
constexpr auto PatchTexture = &TextureLoader::PatchTexture;
#else
constexpr decltype(&TextureLoader::PatchTexture) PatchTexture = nullptr;
#endif
}; // namespace Extensions
