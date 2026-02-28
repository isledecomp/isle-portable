#include "extensions/multiplayer.h"

#include "extensions/multiplayer/networkmanager.h"
#include "extensions/multiplayer/networktransport.h"
#ifdef __EMSCRIPTEN__
#include "extensions/multiplayer/websockettransport.h"
#endif

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
		return;
	}

#ifdef __EMSCRIPTEN__
	s_transport = new Multiplayer::WebSocketTransport(relayUrl);

	s_networkManager = new Multiplayer::NetworkManager();
	s_networkManager->Initialize(s_transport);

	s_networkManager->Connect("default");
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
