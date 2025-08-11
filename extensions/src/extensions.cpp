#include "extensions/extensions.h"

#include "extensions/siloader.h"
#include "extensions/textureloader.h"

#include <SDL3/SDL_log.h>

void Extensions::Enable(const char* p_key, std::map<std::string, std::string> p_options)
{
	for (const char* key : availableExtensions) {
		if (!SDL_strcasecmp(p_key, key)) {
			if (!SDL_strcasecmp(p_key, "extensions:texture loader")) {
				TextureLoader::options = std::move(p_options);
				TextureLoader::enabled = true;
				TextureLoader::Initialize();
			}
			else if (!SDL_strcasecmp(p_key, "extensions:si loader")) {
				SiLoader::options = std::move(p_options);
				SiLoader::enabled = true;
				SiLoader::Initialize();
			}

			SDL_Log("Enabled extension: %s", p_key);
			break;
		}
	}
}
