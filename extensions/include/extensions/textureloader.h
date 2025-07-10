#pragma once

#include "extensions/extensions.h"
#include "legotextureinfo.h"

namespace Extensions
{
class TextureLoader {
public:
	static bool PatchTexture(LegoTextureInfo* p_textureInfo);
	static bool enabled;

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
