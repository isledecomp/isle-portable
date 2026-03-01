#include "extensions/multiplayer.h"

#include "extensions/multiplayer/networkmanager.h"
#include "extensions/multiplayer/networktransport.h"
#include "extensions/multiplayer/protocol.h"
#include "legoactor.h"
#include "legoentity.h"
#include "legogamestate.h"
#include "misc.h"
#ifdef __EMSCRIPTEN__
#include "extensions/multiplayer/websockettransport.h"
#endif

using namespace Extensions;

std::map<std::string, std::string> MultiplayerExt::options;
bool MultiplayerExt::enabled = false;
std::string MultiplayerExt::relayUrl;
std::string MultiplayerExt::room;
Multiplayer::NetworkManager* MultiplayerExt::s_networkManager = nullptr;
Multiplayer::NetworkTransport* MultiplayerExt::s_transport = nullptr;

void MultiplayerExt::Initialize()
{
	relayUrl = options["multiplayer:relay url"];
	room = options["multiplayer:room"];

	if (relayUrl.empty() || room.empty()) {
		return;
	}

#ifdef __EMSCRIPTEN__
	s_transport = new Multiplayer::WebSocketTransport(relayUrl);

	s_networkManager = new Multiplayer::NetworkManager();
	s_networkManager->Initialize(s_transport);

	s_networkManager->Connect(room.c_str());
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
