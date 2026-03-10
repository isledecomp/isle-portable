#pragma once

#include "extensions/extensions.h"
#include "mxtypes.h"

#include <SDL3/SDL_events.h>
#include <map>
#include <string>

class IslePathActor;
class LegoEntity;
class LegoEventNotificationParam;
class LegoPathActor;
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
	static MxBool HandleROIClick(LegoROI* p_rootROI, LegoEventNotificationParam& p_param);

	static std::map<std::string, std::string> options;
	static bool enabled;

	static std::string relayUrl;
	static std::string room;

	static void HandleActorEnter(IslePathActor* p_actor);
	static void HandleActorExit(IslePathActor* p_actor);
	static void HandleCamAnimEnd(LegoPathActor* p_actor);

	// Returns TRUE if the name belongs to a multiplayer clone (entity-less ROI).
	static MxBool IsClonedCharacter(const char* p_name);

	// Called before a save file is loaded. Captures current sky/light state.
	static void HandleBeforeSaveLoad();

	// Called after a save file is loaded. Re-syncs world state with multiplayer peers.
	static void HandleSaveLoaded();

	// Returns true if the multiplayer connection was rejected (e.g. room full).
	static MxBool CheckRejected();

	// Forwards SDL events to the third-person camera for orbit controls.
	static void HandleSDLEvent(SDL_Event* p_event);

	// Returns TRUE when the third-person camera is active.
	static MxBool IsThirdPersonCameraActive();

	// Suppresses touch input when a multi-touch camera gesture is active.
	// Returns TRUE if the caller should return early.
	static MxBool HandleTouchInput();

	static void SetNetworkManager(Multiplayer::NetworkManager* p_networkManager);
	static Multiplayer::NetworkManager* GetNetworkManager();

private:
	static Multiplayer::NetworkManager* s_networkManager;
	static Multiplayer::NetworkTransport* s_transport;
	static Multiplayer::PlatformCallbacks* s_callbacks;
};

#ifdef EXTENSIONS
LEGO1_EXPORT bool IsMultiplayerRejected();

constexpr auto HandleCreate = &MultiplayerExt::HandleCreate;
constexpr auto HandleWorldEnable = &MultiplayerExt::HandleWorldEnable;
constexpr auto HandleEntityNotify = &MultiplayerExt::HandleEntityNotify;
constexpr auto HandleSkyLightControl = &MultiplayerExt::HandleSkyLightControl;
constexpr auto HandleROIClick = &MultiplayerExt::HandleROIClick;
constexpr auto HandleActorEnter = &MultiplayerExt::HandleActorEnter;
constexpr auto HandleActorExit = &MultiplayerExt::HandleActorExit;
constexpr auto HandleCamAnimEnd = &MultiplayerExt::HandleCamAnimEnd;
constexpr auto IsClonedCharacter = &MultiplayerExt::IsClonedCharacter;
constexpr auto HandleBeforeSaveLoad = &MultiplayerExt::HandleBeforeSaveLoad;
constexpr auto HandleSaveLoaded = &MultiplayerExt::HandleSaveLoaded;
constexpr auto CheckRejected = &MultiplayerExt::CheckRejected;
constexpr auto HandleSDLEvent = &MultiplayerExt::HandleSDLEvent;
constexpr auto IsThirdPersonCameraActive = &MultiplayerExt::IsThirdPersonCameraActive;
constexpr auto HandleTouchInput = &MultiplayerExt::HandleTouchInput;
#else
constexpr decltype(&MultiplayerExt::HandleCreate) HandleCreate = nullptr;
constexpr decltype(&MultiplayerExt::HandleWorldEnable) HandleWorldEnable = nullptr;
constexpr decltype(&MultiplayerExt::HandleEntityNotify) HandleEntityNotify = nullptr;
constexpr decltype(&MultiplayerExt::HandleSkyLightControl) HandleSkyLightControl = nullptr;
constexpr decltype(&MultiplayerExt::HandleROIClick) HandleROIClick = nullptr;
constexpr decltype(&MultiplayerExt::HandleActorEnter) HandleActorEnter = nullptr;
constexpr decltype(&MultiplayerExt::HandleActorExit) HandleActorExit = nullptr;
constexpr decltype(&MultiplayerExt::HandleCamAnimEnd) HandleCamAnimEnd = nullptr;
constexpr decltype(&MultiplayerExt::IsClonedCharacter) IsClonedCharacter = nullptr;
constexpr decltype(&MultiplayerExt::HandleBeforeSaveLoad) HandleBeforeSaveLoad = nullptr;
constexpr decltype(&MultiplayerExt::HandleSaveLoaded) HandleSaveLoaded = nullptr;
constexpr decltype(&MultiplayerExt::CheckRejected) CheckRejected = nullptr;
constexpr decltype(&MultiplayerExt::HandleSDLEvent) HandleSDLEvent = nullptr;
constexpr decltype(&MultiplayerExt::IsThirdPersonCameraActive) IsThirdPersonCameraActive = nullptr;
constexpr decltype(&MultiplayerExt::HandleTouchInput) HandleTouchInput = nullptr;
#endif

}; // namespace Extensions
