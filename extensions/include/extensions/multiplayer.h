#pragma once

#include "extensions/extensions.h"
#include "mxtypes.h"

#include <map>
#include <string>

class LegoEntity;
class LegoEventNotificationParam;
class LegoROI;
class LegoWorld;

namespace Multiplayer
{
class NetworkManager;
class NetworkTransport;
class PlatformCallbacks;
} // namespace Multiplayer

namespace Extensions
{

class MultiplayerExt {
public:
	static void Initialize();
	static void HandleCreate();
	static MxBool HandleWorldEnable(LegoWorld* p_world, MxBool p_enable);

	// Intercepts click notifications on plants/buildings for multiplayer routing.
	// Returns TRUE if the click should be suppressed locally (non-host).
	static MxBool HandleEntityNotify(LegoEntity* p_entity);

	// Intercepts observatory sky/light controls for multiplayer routing.
	// Returns TRUE if the local action should be suppressed (non-host).
	static MxBool HandleSkyLightControl(MxU32 p_controlId);

	// Handles clicks on entity-less ROIs (remote players, display actor overrides).
	// When multiplayer is enabled, all customization goes through the network.
	static MxBool HandleROIClick(LegoROI* p_rootROI, LegoEventNotificationParam& p_param);

	static std::map<std::string, std::string> options;
	static bool enabled;

	static MxBool IsClonedCharacter(const char* p_name);
	static void HandleBeforeSaveLoad();
	static void HandleSaveLoaded();
	static MxBool CheckRejected();

	static Multiplayer::NetworkManager* GetNetworkManager();

private:
	static std::string s_relayUrl;
	static std::string s_room;
	static Multiplayer::NetworkManager* s_networkManager;
	static Multiplayer::NetworkTransport* s_transport;
	static Multiplayer::PlatformCallbacks* s_callbacks;
};

#ifdef EXTENSIONS
LEGO1_EXPORT bool IsMultiplayerRejected();
#endif

namespace MP
{
#ifdef EXTENSIONS
constexpr auto HandleCreate = &MultiplayerExt::HandleCreate;
constexpr auto HandleWorldEnable = &MultiplayerExt::HandleWorldEnable;
constexpr auto HandleEntityNotify = &MultiplayerExt::HandleEntityNotify;
constexpr auto HandleSkyLightControl = &MultiplayerExt::HandleSkyLightControl;
constexpr auto HandleROIClick = &MultiplayerExt::HandleROIClick;
constexpr auto IsClonedCharacter = &MultiplayerExt::IsClonedCharacter;
constexpr auto HandleBeforeSaveLoad = &MultiplayerExt::HandleBeforeSaveLoad;
constexpr auto HandleSaveLoaded = &MultiplayerExt::HandleSaveLoaded;
constexpr auto CheckRejected = &MultiplayerExt::CheckRejected;
#else
constexpr decltype(&MultiplayerExt::HandleCreate) HandleCreate = nullptr;
constexpr decltype(&MultiplayerExt::HandleWorldEnable) HandleWorldEnable = nullptr;
constexpr decltype(&MultiplayerExt::HandleEntityNotify) HandleEntityNotify = nullptr;
constexpr decltype(&MultiplayerExt::HandleSkyLightControl) HandleSkyLightControl = nullptr;
constexpr decltype(&MultiplayerExt::HandleROIClick) HandleROIClick = nullptr;
constexpr decltype(&MultiplayerExt::IsClonedCharacter) IsClonedCharacter = nullptr;
constexpr decltype(&MultiplayerExt::HandleBeforeSaveLoad) HandleBeforeSaveLoad = nullptr;
constexpr decltype(&MultiplayerExt::HandleSaveLoaded) HandleSaveLoaded = nullptr;
constexpr decltype(&MultiplayerExt::CheckRejected) CheckRejected = nullptr;
#endif
} // namespace MP

}; // namespace Extensions
