#include "extensions/multiplayer.h"

#include "extensions/multiplayer/charactercustomizer.h"
#include "extensions/multiplayer/networkmanager.h"
#include "extensions/multiplayer/networktransport.h"
#include "extensions/multiplayer/protocol.h"
#include "isle_actions.h"
#include "islepathactor.h"
#include "legoactor.h"
#include "legoactors.h"
#include "legoentity.h"
#include "legoeventnotificationparam.h"
#include "legogamestate.h"
#include "legopathactor.h"
#include "misc.h"
#include "roi/legoroi.h"

#include <SDL3/SDL_stdinc.h>

#ifdef __EMSCRIPTEN__
#include "extensions/multiplayer/platforms/emscripten/callbacks.h"
#include "extensions/multiplayer/platforms/emscripten/websockettransport.h"

#include <emscripten.h>
#endif

using namespace Extensions;

static uint8_t ResolveDisplayActorIndex(const char* p_name)
{
	for (int i = 0; i < static_cast<int>(sizeOfArray(g_actorInfoInit)); i++) {
		if (!SDL_strcasecmp(g_actorInfoInit[i].m_name, p_name)) {
			return static_cast<uint8_t>(i);
		}
	}
	return Multiplayer::DISPLAY_ACTOR_NONE;
}

std::map<std::string, std::string> MultiplayerExt::options;
bool MultiplayerExt::enabled = false;
std::string MultiplayerExt::relayUrl;
std::string MultiplayerExt::room;
Multiplayer::NetworkManager* MultiplayerExt::s_networkManager = nullptr;
Multiplayer::NetworkTransport* MultiplayerExt::s_transport = nullptr;
Multiplayer::PlatformCallbacks* MultiplayerExt::s_callbacks = nullptr;

void MultiplayerExt::Initialize()
{
	relayUrl = options["multiplayer:relay url"];
	room = options["multiplayer:room"];

#ifdef __EMSCRIPTEN__
	s_transport = new Multiplayer::WebSocketTransport(relayUrl);
	s_callbacks = new Multiplayer::EmscriptenCallbacks();

	s_networkManager = new Multiplayer::NetworkManager();
	s_networkManager->Initialize(s_transport, s_callbacks);

	// Third-person camera enabled by default, toggled via WASM export
	s_networkManager->GetThirdPersonCamera().Enable();

	std::string actor = options["multiplayer:actor"];
	if (!actor.empty()) {
		uint8_t displayIndex = ResolveDisplayActorIndex(actor.c_str());
		if (displayIndex != Multiplayer::DISPLAY_ACTOR_NONE) {
			s_networkManager->SetDisplayActorIndex(displayIndex);
		}
	}

	if (!relayUrl.empty() && !room.empty()) {
		s_networkManager->Connect(room.c_str());
	}
#endif
}

void MultiplayerExt::HandleCreate()
{
	if (s_networkManager) {
		s_networkManager->HandleCreate();
	}
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

MxBool MultiplayerExt::HandleROIClick(LegoROI* p_rootROI, LegoEventNotificationParam& p_param)
{
	if (!s_networkManager) {
		return FALSE;
	}

	Multiplayer::NetworkManager* mgr = s_networkManager;

	// Check if it's a remote player
	Multiplayer::RemotePlayer* remote = mgr->FindPlayerByROI(p_rootROI);

	// Check if it's our own 3rd-person display actor override
	bool isSelf =
		(mgr->GetThirdPersonCamera().GetDisplayROI() != nullptr &&
		 mgr->GetThirdPersonCamera().GetDisplayROI() == p_rootROI);

	if (!remote && !isSelf) {
		return FALSE;
	}

	// Remote player permission check
	if (remote && !remote->GetAllowRemoteCustomize()) {
		return TRUE; // Consume click, no effect
	}

	// Determine change type from clicker's actor ID
	uint8_t changeType;
	int partIndex = -1;
	switch (GameState()->GetActorId()) {
	case LegoActor::c_pepper:
		if (GameState()->GetCurrentAct() == LegoGameState::e_act2 ||
			GameState()->GetCurrentAct() == LegoGameState::e_act3) {
			return TRUE;
		}
		changeType = Multiplayer::CHANGE_VARIANT;
		break;
	case LegoActor::c_mama:
		changeType = Multiplayer::CHANGE_SOUND;
		break;
	case LegoActor::c_papa:
		changeType = Multiplayer::CHANGE_MOVE;
		break;
	case LegoActor::c_nick:
		changeType = Multiplayer::CHANGE_COLOR;
		if (p_param.GetROI()) {
			partIndex = Multiplayer::CharacterCustomizer::MapClickedPartIndex(p_param.GetROI()->GetName());
		}
		if (partIndex < 0) {
			return TRUE;
		}
		break;
	case LegoActor::c_laura:
		changeType = Multiplayer::CHANGE_MOOD;
		break;
	case LegoActor::c_brickster:
		return TRUE;
	default:
		return FALSE;
	}

	// Send a customize request to the server. The server echoes it back to all peers
	// (including the sender). HandleCustomize then applies the change and plays effects.
	// For remote targets this avoids flip-flop from stale state messages; for self targets
	// it keeps the code path uniform.
	uint32_t targetPeerId = remote ? remote->GetPeerId() : mgr->GetLocalPeerId();
	mgr->SendCustomize(targetPeerId, changeType, static_cast<uint8_t>(partIndex >= 0 ? partIndex : 0xFF));

	return TRUE;
}

MxBool MultiplayerExt::HandleEntityNotify(LegoEntity* p_entity)
{
	if (!s_networkManager) {
		return FALSE;
	}

	// Only intercept plants and buildings
	MxU8 type = p_entity->GetType();
	if (type != LegoEntity::e_plant && type != LegoEntity::e_building) {
		return FALSE;
	}

	// Determine the change type based on the active character,
	// mirroring the logic in LegoEntity::Notify().
	MxU8 changeType;
	switch (GameState()->GetActorId()) {
	case LegoActor::c_pepper:
		if (GameState()->GetCurrentAct() == LegoGameState::e_act2 ||
			GameState()->GetCurrentAct() == LegoGameState::e_act3) {
			return FALSE;
		}
		changeType = Multiplayer::CHANGE_VARIANT;
		break;
	case LegoActor::c_mama:
		changeType = Multiplayer::CHANGE_SOUND;
		break;
	case LegoActor::c_papa:
		changeType = Multiplayer::CHANGE_MOVE;
		break;
	case LegoActor::c_nick:
		changeType = Multiplayer::CHANGE_COLOR;
		break;
	case LegoActor::c_laura:
		changeType = Multiplayer::CHANGE_MOOD;
		break;
	case LegoActor::c_brickster:
		changeType = Multiplayer::CHANGE_DECREMENT;
		break;
	default:
		return FALSE;
	}

	return s_networkManager->HandleEntityMutation(p_entity, changeType);
}

MxBool MultiplayerExt::HandleSkyLightControl(MxU32 p_controlId)
{
	if (!s_networkManager) {
		return FALSE;
	}

	uint8_t entityType;
	uint8_t changeType;

	switch (p_controlId) {
	case IsleScript::c_Observe_SkyColor_Ctl:
		entityType = Multiplayer::ENTITY_SKY;
		changeType = Multiplayer::SKY_TOGGLE_COLOR;
		break;
	case IsleScript::c_Observe_Sun_Ctl:
		entityType = Multiplayer::ENTITY_SKY;
		changeType = Multiplayer::SKY_DAY;
		break;
	case IsleScript::c_Observe_Moon_Ctl:
		entityType = Multiplayer::ENTITY_SKY;
		changeType = Multiplayer::SKY_NIGHT;
		break;
	case IsleScript::c_Observe_GlobeRArrow_Ctl:
		entityType = Multiplayer::ENTITY_LIGHT;
		changeType = Multiplayer::LIGHT_INCREMENT;
		break;
	case IsleScript::c_Observe_GlobeLArrow_Ctl:
		entityType = Multiplayer::ENTITY_LIGHT;
		changeType = Multiplayer::LIGHT_DECREMENT;
		break;
	default:
		return FALSE;
	}

	return s_networkManager->HandleSkyLightMutation(entityType, changeType);
}

void MultiplayerExt::HandleBeforeSaveLoad()
{
	if (s_networkManager) {
		s_networkManager->OnBeforeSaveLoad();
	}
}

void MultiplayerExt::HandleSaveLoaded()
{
	if (s_networkManager) {
		s_networkManager->OnSaveLoaded();
	}
}

void MultiplayerExt::HandleActorEnter(IslePathActor* p_actor)
{
	if (s_networkManager) {
		s_networkManager->GetThirdPersonCamera().OnActorEnter(p_actor);
	}
}

void MultiplayerExt::HandleActorExit(IslePathActor* p_actor)
{
	if (s_networkManager) {
		s_networkManager->GetThirdPersonCamera().OnActorExit(p_actor);
	}
}

void MultiplayerExt::HandleCamAnimEnd(LegoPathActor* p_actor)
{
	if (s_networkManager) {
		s_networkManager->GetThirdPersonCamera().OnCamAnimEnd(p_actor);
	}
}

MxBool MultiplayerExt::ShouldInvertMovement(LegoPathActor* p_actor)
{
	if (s_networkManager && UserActor() == p_actor) {
		return s_networkManager->GetThirdPersonCamera().IsActive();
	}

	return FALSE;
}

MxBool MultiplayerExt::IsClonedCharacter(const char* p_name)
{
	if (!s_networkManager) {
		return FALSE;
	}

	return s_networkManager->IsClonedCharacter(p_name) ? TRUE : FALSE;
}

void MultiplayerExt::HandleSDLEvent(SDL_Event* p_event)
{
	if (s_networkManager && s_networkManager->GetThirdPersonCamera().IsActive()) {
		s_networkManager->GetThirdPersonCamera().HandleSDLEvent(p_event);
	}
}

MxBool MultiplayerExt::CheckRejected()
{
	if (s_networkManager && s_networkManager->WasRejected()) {
		return TRUE;
	}

	return FALSE;
}

void MultiplayerExt::SetNetworkManager(Multiplayer::NetworkManager* p_networkManager)
{
	s_networkManager = p_networkManager;
}

Multiplayer::NetworkManager* MultiplayerExt::GetNetworkManager()
{
	return s_networkManager;
}

bool Extensions::IsMultiplayerRejected()
{
	return Extension<MultiplayerExt>::Call(CheckRejected).value_or(FALSE);
}
