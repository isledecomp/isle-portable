#include "extensions/extensions.h"

#include "extensions/multiplayer.h"
#include "extensions/siloader.h"
#include "extensions/textureloader.h"
#include "extensions/thirdpersoncamera.h"

#include <SDL3/SDL_log.h>

void Extensions::Enable(const char* p_key, std::map<std::string, std::string> p_options)
{
	for (const char* key : availableExtensions) {
		if (!SDL_strcasecmp(p_key, key)) {
			if (!SDL_strcasecmp(p_key, "extensions:texture loader")) {
				TextureLoaderExt::options = std::move(p_options);
				TextureLoaderExt::enabled = true;
				TextureLoaderExt::Initialize();
			}
			else if (!SDL_strcasecmp(p_key, "extensions:si loader")) {
				SiLoaderExt::options = std::move(p_options);
				SiLoaderExt::enabled = true;
				SiLoaderExt::Initialize();
			}
			else if (!SDL_strcasecmp(p_key, "extensions:third person camera")) {
				ThirdPersonCameraExt::options = std::move(p_options);
				ThirdPersonCameraExt::enabled = true;
				ThirdPersonCameraExt::Initialize();
			}
			else if (!SDL_strcasecmp(p_key, "extensions:multiplayer")) {
				MultiplayerExt::options = std::move(p_options);
				MultiplayerExt::enabled = true;
				MultiplayerExt::Initialize();
			}

			SDL_Log("Enabled extension: %s", p_key);
			break;
		}
	}
}
