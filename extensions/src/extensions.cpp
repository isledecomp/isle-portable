#include "extensions/extensions.h"

#include "extensions/textureloader.h"

#include <SDL3/SDL_log.h>

void Extensions::Enable(const char* p_key)
{
	for (const char* key : availableExtensions) {
		if (!SDL_strcasecmp(p_key, key)) {
			if (!SDL_strcasecmp(p_key, "extensions:texture loader")) {
				TextureLoader::enabled = true;
			}

			SDL_Log("Enabled extension: %s", p_key);
			break;
		}
	}
}
