#include "extensions/multiplayer/networkmanager.h"

#include "extensions/common/animdata.h"
#include "extensions/common/charactercustomizer.h"
#include "extensions/multiplayer/namebubblerenderer.h"
#include "extensions/thirdpersoncamera.h"
#include "extensions/thirdpersoncamera/controller.h"
#include "legoanimationmanager.h"
#include "legocharactermanager.h"
#include "legoextraactor.h"
#include "legogamestate.h"
#include "legomain.h"
#include "legopathactor.h"
#include "legopathcontroller.h"
#include "legoworld.h"
#include "misc.h"
#include "mxmisc.h"
#include "mxticklemanager.h"
#include "roi/legoroi.h"

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <vector>

using namespace Extensions;
using namespace Multiplayer;
using Common::DetectVehicleType;
using Common::IsMultiPartEmote;

template <typename T>
void NetworkManager::SendMessage(const T& p_msg)
{
	if (!m_transport || !m_transport->IsConnected()) {
		return;
	}

	uint8_t buf[sizeof(T)];
	size_t len = SerializeMsg(buf, sizeof(buf), p_msg);
	if (len > 0) {
		m_transport->Send(buf, len);
	}
}

NetworkManager::NetworkManager()
	: m_transport(nullptr), m_callbacks(nullptr), m_localNameBubble(nullptr), m_localPeerId(0), m_hostPeerId(0),
	  m_sequence(0), m_lastBroadcastTime(0), m_lastValidActorId(0), m_localAllowRemoteCustomize(true),
	  m_inIsleWorld(false), m_registered(false), m_pendingToggleThirdPerson(false), m_pendingToggleNameBubbles(false),
	  m_pendingWalkAnim(-1), m_pendingIdleAnim(-1), m_pendingEmote(-1), m_pendingToggleAllowCustomize(false),
	  m_disableAllNPCs(false), m_showNameBubbles(true), m_lastCameraEnabled(false)
{
}

NetworkManager::~NetworkManager()
{
	Shutdown();
}

static ThirdPersonCamera::Controller* GetCamera()
{
	return ThirdPersonCameraExt::GetCamera();
}

MxResult NetworkManager::Tickle()
{
	ProcessPendingRequests();

	if (m_disableAllNPCs) {
		EnforceDisableNPCs();
	}

	// Detect camera state changes for platform notification
	ThirdPersonCamera::Controller* cam = GetCamera();
	if (cam) {
		bool cameraEnabled = cam->IsEnabled();
		if (cameraEnabled != m_lastCameraEnabled) {
			m_lastCameraEnabled = cameraEnabled;
			NotifyThirdPersonChanged(cameraEnabled);

			if (m_localNameBubble) {
				if (!cameraEnabled) {
					m_localNameBubble->SetVisible(false);
				}
				else if (m_showNameBubbles) {
					m_localNameBubble->SetVisible(true);
				}
			}
		}

		// Create local name bubble when display ROI becomes available
		if (m_showNameBubbles && !m_localNameBubble && cam->GetDisplayROI()) {
			char name[8];
			EncodeUsername(name);
			m_localNameBubble = new NameBubbleRenderer();
			m_localNameBubble->Create(name);
		}

		// Update local name bubble position
		if (m_localNameBubble && cam->GetDisplayROI()) {
			m_localNameBubble->Update(cam->GetDisplayROI());
		}
	}

	if (!m_transport) {
		return SUCCESS;
	}

	uint32_t now = SDL_GetTicks();

	// Broadcast before receiving so the Send proxy lets the main thread
	// process WebSocket events before we drain the queue.
	if (m_transport->IsConnected() && (now - m_lastBroadcastTime) >= BROADCAST_INTERVAL_MS) {
		BroadcastLocalState();
		m_lastBroadcastTime = now;
	}

	ProcessIncomingPackets();
	UpdateRemotePlayers(0.016f);

	// Re-read time; ProcessIncomingPackets may have advanced SDL_GetTicks.
	uint32_t timeoutNow = SDL_GetTicks();
	std::vector<uint32_t> timedOut;
	for (auto& [peerId, player] : m_remotePlayers) {
		uint32_t lastUpdate = player->GetLastUpdateTime();
		if (timeoutNow >= lastUpdate && (timeoutNow - lastUpdate) > TIMEOUT_MS) {
			timedOut.push_back(peerId);
		}
	}
	for (uint32_t peerId : timedOut) {
		RemoveRemotePlayer(peerId);
	}

	return SUCCESS;
}

void NetworkManager::Initialize(NetworkTransport* p_transport, PlatformCallbacks* p_callbacks)
{
	m_transport = p_transport;
	m_callbacks = p_callbacks;
	m_worldSync.SetTransport(p_transport);
}

void NetworkManager::HandleCreate()
{
	if (!m_registered) {
		TickleManager()->RegisterClient(this, 10);
		m_registered = true;
	}
}

void NetworkManager::Shutdown()
{
	if (m_transport) {
		Disconnect();
		if (m_registered) {
			TickleManager()->UnregisterClient(this);
			m_registered = false;
		}
		m_transport = nullptr;
		m_worldSync.SetTransport(nullptr);
	}

	delete m_localNameBubble;
	m_localNameBubble = nullptr;

	RemoveAllRemotePlayers();
}

void NetworkManager::Connect(const char* p_roomId)
{
	if (m_transport) {
		m_transport->Connect(p_roomId);
	}
}

void NetworkManager::Disconnect()
{
	if (m_transport) {
		m_transport->Disconnect();
	}
	RemoveAllRemotePlayers();
}

bool NetworkManager::IsConnected() const
{
	return m_transport && m_transport->IsConnected();
}

bool NetworkManager::WasDisconnected() const
{
	return m_transport && m_transport->WasDisconnected();
}

void NetworkManager::OnWorldEnabled(LegoWorld* p_world)
{
	if (!p_world) {
		return;
	}

	if (p_world->GetWorldId() == LegoOmni::e_act1) {
		m_inIsleWorld = true;
		m_worldSync.SetInIsleWorld(true);

		for (auto& [peerId, player] : m_remotePlayers) {
			if (player->IsSpawned()) {
				player->ReAddToScene();
			}
			else {
				player->Spawn(p_world);
				if (player->GetROI()) {
					m_roiToPlayer[player->GetROI()] = player.get();
				}
			}

			if (player->IsSpawned() && player->GetWorldId() == (int8_t) LegoOmni::e_act1) {
				player->SetVisible(true);
				player->SetNameBubbleVisible(m_showNameBubbles);
			}
		}

		NotifyPlayerCountChanged();

		if (m_disableAllNPCs) {
			EnforceDisableNPCs();
		}
	}
}

void NetworkManager::OnWorldDisabled(LegoWorld* p_world)
{
	if (!p_world) {
		return;
	}

	if (p_world->GetWorldId() == LegoOmni::e_act1) {
		m_inIsleWorld = false;
		m_worldSync.SetInIsleWorld(false);

		// Destroy local name bubble (ROI is about to be destroyed)
		if (m_localNameBubble) {
			m_localNameBubble->Destroy();
			delete m_localNameBubble;
			m_localNameBubble = nullptr;
		}

		for (auto& [peerId, player] : m_remotePlayers) {
			player->SetVisible(false);
			player->SetNameBubbleVisible(false);
		}

		NotifyPlayerCountChanged();
	}
}

void NetworkManager::OnBeforeSaveLoad()
{
	if (m_transport && m_transport->IsConnected() && !IsHost()) {
		m_worldSync.SaveSkyLightState();
	}
}

void NetworkManager::OnSaveLoaded()
{
	if (!m_transport || !m_transport->IsConnected()) {
		return;
	}

	// After a save file load, the local plant/building/sky/light state comes
	// from the save file and may diverge from the host's state.
	// Host broadcasts to all peers (targetPeerId=0); non-host restores the
	// pre-load sky/light to avoid visual flicker, then requests a fresh snapshot.
	if (IsHost()) {
		m_worldSync.SendWorldSnapshotTo(0);
	}
	else {
		m_worldSync.RestoreSkyLightState();
		m_worldSync.OnHostChanged();
	}
}

MxBool NetworkManager::HandleEntityMutation(LegoEntity* p_entity, MxU8 p_changeType)
{
	return m_worldSync.HandleEntityMutation(p_entity, p_changeType);
}

MxBool NetworkManager::HandleSkyLightMutation(uint8_t p_entityType, uint8_t p_changeType)
{
	return m_worldSync.HandleSkyLightMutation(p_entityType, p_changeType);
}

void NetworkManager::EnforceDisableNPCs()
{
	LegoAnimationManager* am = AnimationManager();
	if (!am) {
		return;
	}

	am->m_numAllowedExtras = 0;
	am->m_enableCamAnims = FALSE;
	am->m_unk0x400 = FALSE;

	// Purge all extras including ambient NPCs (mama, papa, brickster)
	// that are spawned by camera path triggers via FUN_10064380.
	// PurgeExtra(TRUE) deliberately skips mama/papa, so we purge manually.
	for (MxS32 i = 0; i < (MxS32) sizeOfArray(am->m_extras); i++) {
		if (am->m_extras[i].m_roi != NULL) {
			LegoPathActor* actor = CharacterManager()->GetExtraActor(am->m_extras[i].m_roi->GetName());
			if (actor != NULL && actor->GetController() != NULL) {
				actor->GetController()->RemoveActor(actor);
				actor->SetController(NULL);
			}

			CharacterManager()->ReleaseActor(am->m_extras[i].m_roi);
			am->m_extras[i].m_roi = NULL;
			am->m_extras[i].m_characterId = -1;
			am->m_unk0x414--;
		}
	}
}

void NetworkManager::ProcessPendingRequests()
{
	ThirdPersonCamera::Controller* cam = GetCamera();

	// Camera-dependent requests: only consume when cam is available so
	// the request survives until the camera exists.
	if (cam) {
		if (m_pendingToggleThirdPerson.exchange(false, std::memory_order_relaxed)) {
			if (cam->IsEnabled()) {
				cam->Disable();
			}
			else {
				cam->Enable();
			}
			NotifyThirdPersonChanged(cam->IsEnabled());
		}

		int walkAnim = m_pendingWalkAnim.exchange(-1, std::memory_order_relaxed);
		if (walkAnim >= 0) {
			SetWalkAnimation(static_cast<uint8_t>(walkAnim));
		}

		int idleAnim = m_pendingIdleAnim.exchange(-1, std::memory_order_relaxed);
		if (idleAnim >= 0) {
			SetIdleAnimation(static_cast<uint8_t>(idleAnim));
		}

		int emote = m_pendingEmote.exchange(-1, std::memory_order_relaxed);
		if (emote >= 0) {
			SendEmote(static_cast<uint8_t>(emote));
		}
	}

	if (m_pendingToggleAllowCustomize.exchange(false, std::memory_order_relaxed)) {
		m_localAllowRemoteCustomize = !m_localAllowRemoteCustomize;
		NotifyAllowCustomizeChanged(m_localAllowRemoteCustomize);
	}

	if (m_pendingToggleNameBubbles.exchange(false, std::memory_order_relaxed)) {
		m_showNameBubbles = !m_showNameBubbles;
		for (auto& [peerId, player] : m_remotePlayers) {
			player->SetNameBubbleVisible(m_showNameBubbles);
		}
		if (m_localNameBubble) {
			m_localNameBubble->SetVisible(m_showNameBubbles);
		}
		NotifyNameBubblesChanged(m_showNameBubbles);
	}
}

void NetworkManager::BroadcastLocalState()
{
	if (!m_transport) {
		return;
	}

	LegoPathActor* userActor = UserActor();
	LegoWorld* currentWorld = CurrentWorld();

	if (!userActor || !currentWorld) {
		return;
	}

	LegoROI* roi = userActor->GetROI();
	if (!roi) {
		return;
	}

	const float* pos = roi->GetWorldPosition();
	const float* dir = roi->GetWorldDirection();
	const float* up = roi->GetWorldUp();
	float speed = userActor->GetWorldSpeed();

	uint8_t actorId = static_cast<LegoActor*>(userActor)->GetActorId();
	if (IsValidActorId(actorId)) {
		m_lastValidActorId = actorId;
	}
	else {
		actorId = m_lastValidActorId;
	}

	if (!IsValidActorId(actorId)) {
		return;
	}

	ThirdPersonCamera::Controller* cam = GetCamera();

	PlayerStateMsg msg{};
	msg.header = {MSG_STATE, m_localPeerId, m_sequence++, TARGET_BROADCAST};
	msg.actorId = actorId;
	msg.worldId = (int8_t) currentWorld->GetWorldId();
	msg.vehicleType = DetectVehicleType(userActor);
	SDL_memcpy(msg.position, pos, sizeof(msg.position));
	SDL_memcpy(msg.direction, dir, sizeof(msg.direction));
	SDL_memcpy(msg.up, up, sizeof(msg.up));
	msg.speed = speed;

	EncodeUsername(msg.name);

	if (cam) {
		msg.walkAnimId = cam->GetWalkAnimId();
		msg.idleAnimId = cam->GetIdleAnimId();
		msg.displayActorIndex = cam->GetDisplayActorIndex();
		cam->GetCustomizeState().Pack(msg.customizeData);

		// Encode multi-part emote frozen state (0x02 = frozen, emote ID in bits 2-4, max 8 emotes)
		int8_t frozenId = cam->GetFrozenEmoteId();
		if (frozenId >= 0) {
			msg.customizeFlags |= 0x02;
			msg.customizeFlags |= (frozenId & 0x07) << 2;
		}

		// Zero speed when in any phase of a multi-part emote
		if (cam->IsInMultiPartEmote()) {
			msg.speed = 0.0f;
		}
	}

	msg.customizeFlags |= m_localAllowRemoteCustomize ? 0x01 : 0x00;

	SendMessage(msg);
}

void NetworkManager::ProcessIncomingPackets()
{
	if (!m_transport) {
		return;
	}

	m_transport->Receive([this](const uint8_t* data, size_t length) {
		uint8_t msgType = ParseMessageType(data, length);

		switch (msgType) {
		case MSG_ASSIGN_ID: {
			if (length >= 5) {
				uint32_t assignedId;
				SDL_memcpy(&assignedId, data + 1, sizeof(uint32_t));
				m_localPeerId = assignedId;
				m_worldSync.SetLocalPeerId(assignedId);
			}
			if (length >= 6) {
				uint8_t maxActors = data[5];
				if (maxActors <= 40) {
					LegoAnimationManager::configureLegoAnimationManager(maxActors);
					if (AnimationManager()) {
						AnimationManager()->m_maxAllowedExtras = maxActors;
						AnimationManager()->m_numAllowedExtras =
							SDL_min(AnimationManager()->m_numAllowedExtras, (MxU32) maxActors);
					}
					m_disableAllNPCs = (maxActors == 0);
					if (m_disableAllNPCs) {
						EnforceDisableNPCs();
					}
				}
			}
			break;
		}
		case MSG_HOST_ASSIGN: {
			HostAssignMsg msg;
			if (DeserializeMsg(data, length, msg) && msg.header.type == MSG_HOST_ASSIGN) {
				HandleHostAssign(msg);
			}
			break;
		}
		case MSG_LEAVE: {
			PlayerLeaveMsg msg;
			if (DeserializeMsg(data, length, msg) && msg.header.type == MSG_LEAVE) {
				HandleLeave(msg);
			}
			break;
		}
		case MSG_STATE: {
			PlayerStateMsg msg;
			if (DeserializeMsg(data, length, msg) && msg.header.type == MSG_STATE) {
				HandleState(msg);
			}
			break;
		}
		case MSG_REQUEST_SNAPSHOT: {
			RequestSnapshotMsg msg;
			if (DeserializeMsg(data, length, msg) && msg.header.type == MSG_REQUEST_SNAPSHOT) {
				m_worldSync.HandleRequestSnapshot(msg);
			}
			break;
		}
		case MSG_WORLD_SNAPSHOT: {
			m_worldSync.HandleWorldSnapshot(data, length);
			break;
		}
		case MSG_WORLD_EVENT: {
			WorldEventMsg msg;
			if (DeserializeMsg(data, length, msg) && msg.header.type == MSG_WORLD_EVENT) {
				m_worldSync.HandleWorldEvent(msg);
			}
			break;
		}
		case MSG_WORLD_EVENT_REQUEST: {
			WorldEventRequestMsg msg;
			if (DeserializeMsg(data, length, msg) && msg.header.type == MSG_WORLD_EVENT_REQUEST) {
				m_worldSync.HandleWorldEventRequest(msg);
			}
			break;
		}
		case MSG_EMOTE: {
			EmoteMsg msg;
			if (DeserializeMsg(data, length, msg) && msg.header.type == MSG_EMOTE) {
				HandleEmote(msg);
			}
			break;
		}
		case MSG_CUSTOMIZE: {
			CustomizeMsg msg;
			if (DeserializeMsg(data, length, msg) && msg.header.type == MSG_CUSTOMIZE) {
				HandleCustomize(msg);
			}
			break;
		}
		default:
			break;
		}
	});
}

void NetworkManager::UpdateRemotePlayers(float p_deltaTime)
{
	for (auto& [peerId, player] : m_remotePlayers) {
		player->Tick(p_deltaTime);
	}
}

RemotePlayer* NetworkManager::CreateAndSpawnPlayer(uint32_t p_peerId, uint8_t p_actorId, uint8_t p_displayActorIndex)
{
	auto player = std::make_unique<RemotePlayer>(p_peerId, p_actorId, p_displayActorIndex);

	if (m_inIsleWorld) {
		LegoWorld* world = CurrentWorld();
		if (world && world->GetWorldId() == LegoOmni::e_act1) {
			player->Spawn(world);
		}
	}

	RemotePlayer* ptr = player.get();
	m_remotePlayers[p_peerId] = std::move(player);

	if (ptr->GetROI()) {
		m_roiToPlayer[ptr->GetROI()] = ptr;
	}

	return ptr;
}

void NetworkManager::HandleLeave(const PlayerLeaveMsg& p_msg)
{
	RemoveRemotePlayer(p_msg.header.peerId);
}

void NetworkManager::HandleState(const PlayerStateMsg& p_msg)
{
	uint32_t peerId = p_msg.header.peerId;

	auto it = m_remotePlayers.find(peerId);
	if (it == m_remotePlayers.end()) {
		if (!IsValidActorId(p_msg.actorId)) {
			return;
		}

		CreateAndSpawnPlayer(peerId, p_msg.actorId, p_msg.displayActorIndex);
		NotifyPlayerCountChanged();
		it = m_remotePlayers.find(peerId);
	}

	// Respawn only if display actor changed (not on actorId change)
	if (it->second->GetDisplayActorIndex() != p_msg.displayActorIndex) {
		if (it->second->GetROI()) {
			m_roiToPlayer.erase(it->second->GetROI());
		}
		it->second->Despawn();
		m_remotePlayers.erase(it);
		CreateAndSpawnPlayer(peerId, p_msg.actorId, p_msg.displayActorIndex);
		it = m_remotePlayers.find(peerId);
	}
	else if (IsValidActorId(p_msg.actorId)) {
		it->second->SetActorId(p_msg.actorId); // Update for future use, no visual change
	}

	int8_t oldWorldId = it->second->GetWorldId();

	it->second->UpdateFromNetwork(p_msg);

	bool bothInIsle = m_inIsleWorld && (p_msg.worldId == (int8_t) LegoOmni::e_act1);
	if (it->second->IsSpawned()) {
		it->second->SetVisible(bothInIsle);
		it->second->SetNameBubbleVisible(bothInIsle && m_showNameBubbles);
	}

	bool wasInIsle = (oldWorldId == (int8_t) LegoOmni::e_act1);
	bool nowInIsle = (p_msg.worldId == (int8_t) LegoOmni::e_act1);
	if (m_inIsleWorld && wasInIsle != nowInIsle) {
		NotifyPlayerCountChanged();
	}
}

void NetworkManager::HandleHostAssign(const HostAssignMsg& p_msg)
{
	uint32_t oldHost = m_hostPeerId;
	m_hostPeerId = p_msg.hostPeerId;

	m_worldSync.SetHost(IsHost());

	if (!IsHost() && oldHost != m_hostPeerId) {
		m_worldSync.OnHostChanged();
	}
}

void NetworkManager::SetWalkAnimation(uint8_t p_walkAnimId)
{
	ThirdPersonCamera::Controller* cam = GetCamera();
	if (cam && p_walkAnimId < Common::g_walkAnimCount) {
		cam->SetWalkAnimId(p_walkAnimId);
	}
}

void NetworkManager::SetIdleAnimation(uint8_t p_idleAnimId)
{
	ThirdPersonCamera::Controller* cam = GetCamera();
	if (cam && p_idleAnimId < Common::g_idleAnimCount) {
		cam->SetIdleAnimId(p_idleAnimId);
	}
}

void NetworkManager::SendEmote(uint8_t p_emoteId)
{
	if (p_emoteId >= Common::g_emoteAnimCount) {
		return;
	}

	ThirdPersonCamera::Controller* cam = GetCamera();
	if (!cam) {
		return;
	}

	// Multi-part emotes require 3rd person camera to be active (they need the display clone).
	// In 1st person mode, skip them entirely to avoid broadcasting an emote the local player can't play.
	if (!cam->IsActive() && IsMultiPartEmote(p_emoteId)) {
		return;
	}

	cam->TriggerEmote(p_emoteId);

	EmoteMsg msg{};
	msg.header = {MSG_EMOTE, m_localPeerId, m_sequence++, TARGET_BROADCAST};
	msg.emoteId = p_emoteId;
	SendMessage(msg);
}

void NetworkManager::HandleEmote(const EmoteMsg& p_msg)
{
	uint32_t peerId = p_msg.header.peerId;
	auto it = m_remotePlayers.find(peerId);
	if (it != m_remotePlayers.end()) {
		it->second->TriggerEmote(p_msg.emoteId);
	}
}

void NetworkManager::RemoveRemotePlayer(uint32_t p_peerId)
{
	auto it = m_remotePlayers.find(p_peerId);
	if (it != m_remotePlayers.end()) {
		if (it->second->GetROI()) {
			m_roiToPlayer.erase(it->second->GetROI());
		}
		it->second->Despawn();
		m_remotePlayers.erase(it);
		NotifyPlayerCountChanged();
	}
}

void NetworkManager::RemoveAllRemotePlayers()
{
	for (auto& [peerId, player] : m_remotePlayers) {
		player->Despawn();
	}
	m_remotePlayers.clear();
	m_roiToPlayer.clear();
	NotifyPlayerCountChanged();
}

void NetworkManager::NotifyPlayerCountChanged()
{
	if (!m_callbacks) {
		return;
	}

	int count = -1;
	if (m_inIsleWorld) {
		count = 0;

		// Only count the local player if they have a valid actor.
		// UserActor() can be temporarily NULL during world transitions
		// (e.g. returning from a race, where LegoRace stashes the actor
		// and only restores it in its destructor). Fall back to the
		// GameState actorId which is restored earlier.
		LegoPathActor* userActor = UserActor();
		if (userActor && IsValidActorId(static_cast<LegoActor*>(userActor)->GetActorId())) {
			count = 1;
		}
		else if (IsValidActorId(GameState()->GetActorId())) {
			count = 1;
		}

		for (auto& [peerId, player] : m_remotePlayers) {
			if (player->GetWorldId() == (int8_t) LegoOmni::e_act1) {
				count++;
			}
		}
	}

	m_callbacks->OnPlayerCountChanged(count);
}

void NetworkManager::NotifyThirdPersonChanged(bool p_enabled)
{
	if (!m_callbacks) {
		return;
	}

	m_callbacks->OnThirdPersonChanged(p_enabled);
}

void NetworkManager::NotifyNameBubblesChanged(bool p_enabled)
{
	if (!m_callbacks) {
		return;
	}

	m_callbacks->OnNameBubblesChanged(p_enabled);
}

void NetworkManager::NotifyAllowCustomizeChanged(bool p_enabled)
{
	if (!m_callbacks) {
		return;
	}

	m_callbacks->OnAllowCustomizeChanged(p_enabled);
}

RemotePlayer* NetworkManager::FindPlayerByROI(LegoROI* roi) const
{
	auto it = m_roiToPlayer.find(roi);
	if (it != m_roiToPlayer.end()) {
		return it->second;
	}
	return nullptr;
}

bool NetworkManager::IsClonedCharacter(const char* p_name) const
{
	// Check remote player clones
	for (auto& it : m_remotePlayers) {
		if (!SDL_strcasecmp(it.second->GetUniqueName(), p_name)) {
			return true;
		}
	}

	return false;
}

void NetworkManager::SendCustomize(uint32_t p_targetPeerId, uint8_t p_changeType, uint8_t p_partIndex)
{
	CustomizeMsg msg{};
	msg.header = {MSG_CUSTOMIZE, m_localPeerId, m_sequence++, TARGET_BROADCAST_ALL};
	msg.targetPeerId = p_targetPeerId;
	msg.changeType = p_changeType;
	msg.partIndex = p_partIndex;
	SendMessage(msg);
}

void NetworkManager::HandleCustomize(const CustomizeMsg& p_msg)
{
	uint32_t targetPeerId = p_msg.targetPeerId;

	// Check if the target is a remote player on this client.
	// Only play effects here -- do NOT modify the remote player's customize state.
	// State changes come exclusively through UpdateFromNetwork (from the target's
	// authoritative PlayerStateMsg), which prevents flip-flop from stale state messages.
	// Note: sound/mood feedback uses the old state (before the authoritative update arrives),
	// so the played sound may lag one step behind. This is an accepted tradeoff.
	auto it = m_remotePlayers.find(targetPeerId);
	if (it != m_remotePlayers.end()) {
		if (it->second->GetROI()) {
			Common::CharacterCustomizer::PlayClickSound(
				it->second->GetROI(),
				it->second->GetCustomizeState(),
				p_msg.changeType == CHANGE_MOOD
			);
			if (!it->second->IsMoving() && !it->second->IsInMultiPartEmote()) {
				it->second->StopClickAnimation();
				MxU32 clickAnimId = Common::CharacterCustomizer::PlayClickAnimation(
					it->second->GetROI(),
					it->second->GetCustomizeState()
				);
				it->second->SetClickAnimObjectId(clickAnimId);
			}
		}
		return;
	}

	// Check if the target is the local player
	if (targetPeerId == m_localPeerId) {
		// Reject remote customization if not allowed
		if (p_msg.header.peerId != m_localPeerId && !m_localAllowRemoteCustomize) {
			return;
		}

		ThirdPersonCamera::Controller* cam = GetCamera();
		if (!cam) {
			return;
		}

		// ApplyCustomizeChange handles null display ROI (advances state without visual)
		cam->ApplyCustomizeChange(p_msg.changeType, p_msg.partIndex);

		// Use display ROI for effects in 3rd person, native ROI in 1st person
		LegoROI* effectROI = cam->GetDisplayROI();
		if (!effectROI && UserActor()) {
			effectROI = UserActor()->GetROI();
		}

		if (effectROI) {
			Common::CharacterCustomizer::PlayClickSound(
				effectROI,
				cam->GetCustomizeState(),
				p_msg.changeType == CHANGE_MOOD
			);

			// Only play click animation in 3rd person (not visible in 1st person or multi-part emote)
			if (cam->GetDisplayROI() && !cam->IsInVehicle() && !cam->IsInMultiPartEmote()) {
				cam->StopClickAnimation();
				MxU32 clickAnimId =
					Common::CharacterCustomizer::PlayClickAnimation(cam->GetDisplayROI(), cam->GetCustomizeState());
				cam->SetClickAnimObjectId(clickAnimId);
			}
		}
	}
}
