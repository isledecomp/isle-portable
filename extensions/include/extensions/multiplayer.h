#pragma once

#include "extensions/extensions.h"
#include "mxtypes.h"

#include <map>
#include <string>

class LegoWorld;

namespace Multiplayer
{
class NetworkManager;
class NetworkTransport;
} // namespace Multiplayer

namespace Extensions
{

class MultiplayerExt {
public:
	static void Initialize();
	static MxBool HandleWorldEnable(LegoWorld* p_world, MxBool p_enable);

	static std::map<std::string, std::string> options;
	static bool enabled;

	static std::string relayUrl;

	static void SetNetworkManager(Multiplayer::NetworkManager* p_mgr);
	static Multiplayer::NetworkManager* GetNetworkManager();

private:
	static Multiplayer::NetworkManager* s_networkManager;
	static Multiplayer::NetworkTransport* s_transport;
};

#ifdef EXTENSIONS
constexpr auto HandleWorldEnable = &MultiplayerExt::HandleWorldEnable;
#else
constexpr decltype(&MultiplayerExt::HandleWorldEnable) HandleWorldEnable = nullptr;
#endif

}; // namespace Extensions
