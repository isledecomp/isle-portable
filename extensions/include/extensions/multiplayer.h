#pragma once

#include "extensions/extensions.h"
#include "mxtypes.h"

#include <map>
#include <string>

class LegoEntity;
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

	// Intercepts click notifications on plants/buildings for multiplayer routing.
	// Returns TRUE if the click should be suppressed locally (non-host).
	static MxBool HandleEntityNotify(LegoEntity* p_entity);

	static std::map<std::string, std::string> options;
	static bool enabled;

	static std::string relayUrl;
	static std::string room;

	// Returns true if the multiplayer connection was rejected (e.g. room full).
	static MxBool CheckRejected();

	static void SetNetworkManager(Multiplayer::NetworkManager* p_networkManager);
	static Multiplayer::NetworkManager* GetNetworkManager();

private:
	static Multiplayer::NetworkManager* s_networkManager;
	static Multiplayer::NetworkTransport* s_transport;
};

#ifdef EXTENSIONS
constexpr auto HandleWorldEnable = &MultiplayerExt::HandleWorldEnable;
constexpr auto HandleEntityNotify = &MultiplayerExt::HandleEntityNotify;
constexpr auto CheckRejected = &MultiplayerExt::CheckRejected;
#else
constexpr decltype(&MultiplayerExt::HandleWorldEnable) HandleWorldEnable = nullptr;
constexpr decltype(&MultiplayerExt::HandleEntityNotify) HandleEntityNotify = nullptr;
constexpr decltype(&MultiplayerExt::CheckRejected) CheckRejected = nullptr;
#endif

}; // namespace Extensions
