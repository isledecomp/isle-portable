#include "extensions/extensions.h"

#include "extensions/multiplayer.h"
#include "extensions/siloader.h"
#include "extensions/textureloader.h"
#include "extensions/thirdpersoncamera.h"

#include <SDL3/SDL_log.h>

using namespace Extensions;

static void InitTextureLoader(std::map<std::string, std::string> p_options)
{
	TextureLoaderExt::options = std::move(p_options);
	TextureLoaderExt::enabled = true;
	TextureLoaderExt::Initialize();
}

static void InitSiLoader(std::map<std::string, std::string> p_options)
{
	SiLoaderExt::options = std::move(p_options);
	SiLoaderExt::enabled = true;
	SiLoaderExt::Initialize();
}

static void InitThirdPersonCamera(std::map<std::string, std::string> p_options)
{
	ThirdPersonCameraExt::options = std::move(p_options);
	ThirdPersonCameraExt::enabled = true;
	ThirdPersonCameraExt::Initialize();
}

static void InitMultiplayer(std::map<std::string, std::string> p_options)
{
	MultiplayerExt::options = std::move(p_options);
	MultiplayerExt::enabled = true;
	MultiplayerExt::Initialize();
}

using InitFn = void (*)(std::map<std::string, std::string>);

static const InitFn extensionInits[] = {InitTextureLoader, InitSiLoader, InitThirdPersonCamera, InitMultiplayer};

void Extensions::Enable(const char* p_key, std::map<std::string, std::string> p_options)
{
	for (int i = 0; i < (int) (sizeof(availableExtensions) / sizeof(availableExtensions[0])); i++) {
		if (!SDL_strcasecmp(p_key, availableExtensions[i])) {
			extensionInits[i](std::move(p_options));
			SDL_Log("Enabled extension: %s", p_key);
			return;
		}
	}
}
