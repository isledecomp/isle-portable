#include "extensions/multiplayer.h"

#include "extensions/multiplayer/networkmanager.h"
#include "extensions/multiplayer/networktransport.h"
#ifdef __EMSCRIPTEN__
#include "extensions/multiplayer/websockettransport.h"
#endif

#include <SDL3/SDL_log.h>

using namespace Extensions;

std::map<std::string, std::string> MultiplayerExt::options;
bool MultiplayerExt::enabled = false;
std::string MultiplayerExt::relayUrl;
Multiplayer::NetworkManager* MultiplayerExt::s_networkManager = nullptr;
Multiplayer::NetworkTransport* MultiplayerExt::s_transport = nullptr;

void MultiplayerExt::Initialize()
{
	relayUrl = options["multiplayer:relay url"];

	if (relayUrl.empty()) {
		SDL_Log("Multiplayer: no relay url configured, multiplayer will not connect");
		return;
	}

#ifdef __EMSCRIPTEN__
	s_transport = new Multiplayer::WebSocketTransport(relayUrl);

	s_networkManager = new Multiplayer::NetworkManager();
	s_networkManager->Initialize(s_transport);

	// Auto-connect to default room for MVP
	s_networkManager->Connect("default");

	SDL_Log("Multiplayer: initialized with relay url %s", relayUrl.c_str());
#else
	SDL_Log("Multiplayer: no transport available for this platform yet");
#endif
}

MxBool MultiplayerExt::HandleWorldEnable(LegoWorld* p_world, MxBool p_enable)
{
	if (!s_networkManager) {
		return FALSE;
	}

	if (p_enable) {
		s_networkManager->OnWorldEnabled(p_world);
	}
	else {
		s_networkManager->OnWorldDisabled(p_world);
	}

	return TRUE;
}

void MultiplayerExt::SetNetworkManager(Multiplayer::NetworkManager* p_mgr)
{
	s_networkManager = p_mgr;
}

Multiplayer::NetworkManager* MultiplayerExt::GetNetworkManager()
{
	return s_networkManager;
}
