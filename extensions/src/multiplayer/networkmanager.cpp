#include "extensions/multiplayer/networkmanager.h"

#include "extensions/common/arearestriction.h"
#include "extensions/common/charactercustomizer.h"
#include "extensions/common/charactertables.h"
#include "extensions/multiplayer/namebubblerenderer.h"
#include "extensions/thirdpersoncamera.h"
#include "extensions/thirdpersoncamera/controller.h"
#include "legoactor.h"
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

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <algorithm>
#include <set>
#include <vector>

using namespace Extensions;
using namespace Multiplayer;
using Common::DetectVehicleType;
using Common::IsMultiPartEmote;
using Common::IsRestrictedArea;
using Common::WORLD_NOT_VISIBLE;

// Slightly larger than NPC_ANIM_PROXIMITY to catch transitions
static constexpr float NPC_ANIM_NEARBY_RADIUS_SQ =
	(Animation::NPC_ANIM_PROXIMITY + 5.0f) * (Animation::NPC_ANIM_PROXIMITY + 5.0f);

static const char* IDLE_ANIM_STATE_JSON =
	"{\"locations\":[],\"state\":0,\"currentAnimIndex\":65535,\"pendingInterest\":-1,\"animations\":[]}";

static void ExtractSlotPeerIds(const AnimUpdateMsg& p_msg, uint32_t p_out[8])
{
	for (uint8_t i = 0; i < 8; i++) {
		p_out[i] = (i < p_msg.slotCount) ? p_msg.slots[i].peerId : 0;
	}
}

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
	  m_pendingAnimInterest(-1), m_pendingAnimCancel(false), m_localPendingAnimInterest(-1), m_showNameBubbles(true),
	  m_lastCameraEnabled(false), m_lastVehicleState(0), m_vehicleFilterLogPending(false),
	  m_wasInRestrictedArea(false), m_animStateDirty(false),
	  m_animInterestDirty(false), m_lastAnimPushTime(0), m_connectionState(STATE_DISCONNECTED), m_wasRejected(false),
	  m_reconnectAttempt(0), m_reconnectDelay(0), m_nextReconnectTime(0)
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
	CheckConnectionState();

	EnforceDisableNPCs();

	// Detect camera state changes for platform notification
	ThirdPersonCamera::Controller* cam = GetCamera();
	if (cam) {
		bool cameraEnabled = cam->IsEnabled();
		if (cameraEnabled != m_lastCameraEnabled) {
			m_lastCameraEnabled = cameraEnabled;
			m_animStateDirty = true;
			NotifyThirdPersonChanged(cameraEnabled);

			// Cancel animation when camera is disabled (vehicle entry, restricted area, etc.)
			if (!cameraEnabled && m_animCoordinator.GetState() != Animation::CoordinationState::e_idle) {
				uint16_t localAnim = m_animCoordinator.GetCurrentAnimIndex();
				CancelLocalAnimInterest();
				if (localAnim != Animation::ANIM_INDEX_NONE) {
					StopScenePlayback(localAnim, false);
				}
			}

			if (m_localNameBubble) {
				if (!cameraEnabled) {
					m_localNameBubble->SetVisible(false);
				}
				else if (m_showNameBubbles) {
					m_localNameBubble->SetVisible(true);
				}
			}
		}

		// Detect vehicle state changes for animation eligibility refresh.
		// Tracks three states: on foot, on own vehicle, on foreign vehicle.
		int8_t localChar = Animation::Catalog::DisplayActorToCharacterIndex(cam->GetDisplayActorIndex());
		uint8_t vehicleState = Animation::Catalog::GetVehicleState(localChar, cam->GetRideVehicleROI());
		if (vehicleState != m_lastVehicleState) {
			m_lastVehicleState = vehicleState;
			m_animStateDirty = true;
			m_vehicleFilterLogPending = true;

			// Cancel active session if the current animation is no longer eligible.
			// Only cancel if the local player is a performer — spectators aren't vehicle-constrained.
			if (m_animCoordinator.GetState() != Animation::CoordinationState::e_idle) {
				uint16_t currentAnim = m_animCoordinator.GetCurrentAnimIndex();
				if (currentAnim != Animation::ANIM_INDEX_NONE) {
					const Animation::CatalogEntry* entry = m_animCatalog.FindEntry(currentAnim);
					if (entry && (entry->performerMask >> localChar) & 1) {
						if (!Animation::Catalog::CheckVehicleEligibility(entry, localChar, vehicleState)) {
							CancelLocalAnimInterest();
							StopScenePlayback(currentAnim, false);
						}
					}
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

	// Update local player location proximity
	if (m_inIsleWorld) {
		LegoPathActor* userActor = UserActor();
		if (userActor && userActor->GetROI()) {
			const float* pos = userActor->GetROI()->GetWorldPosition();
			if (m_locationProximity.Update(pos[0], pos[2])) {
				m_animStateDirty = true;

				Animation::CoordinationState oldState = m_animCoordinator.GetState();
				m_animCoordinator.OnLocationChanged(m_locationProximity.GetLocations(), &m_animCatalog);

				// Location change cleared interest — send cancel to host
				if (oldState != Animation::CoordinationState::e_idle &&
					m_animCoordinator.GetState() == Animation::CoordinationState::e_idle) {
					if (IsHost()) {
						HandleAnimCancel(m_localPeerId);
					}
					else if (IsConnected()) {
						AnimCancelMsg cancelMsg{};
						cancelMsg.header = {MSG_ANIM_CANCEL, m_localPeerId, m_sequence++, TARGET_HOST};
						SendMessage(cancelMsg);
					}
					m_localPendingAnimInterest = -1;
				}
			}
		}

		if (IsHost()) {
			TickHostSessions();
		}
		else if (m_animCoordinator.GetState() == Animation::CoordinationState::e_countdown) {
			m_animStateDirty = true;
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
	TickAnimation();

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

	// Push animation state to frontend if dirty (throttled)
	if (m_animStateDirty && m_inIsleWorld && m_callbacks) {
		uint32_t pushNow = SDL_GetTicks();
		bool cooldownExpired = (pushNow - m_lastAnimPushTime) >= ANIM_PUSH_COOLDOWN_MS;
		if (cooldownExpired || m_animInterestDirty) {
			m_animStateDirty = false;
			m_animInterestDirty = false;
			m_lastAnimPushTime = pushNow;
			PushAnimationState();
		}
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
		m_roomId = p_roomId;
		m_transport->Connect(p_roomId);
		m_connectionState = STATE_CONNECTED;
	}
}

void NetworkManager::Disconnect()
{
	m_connectionState = STATE_DISCONNECTED;
	if (m_transport) {
		m_transport->Disconnect();
	}
	RemoveAllRemotePlayers();
	ResetAnimationState();
}

bool NetworkManager::IsConnected() const
{
	return m_transport && m_transport->IsConnected();
}

bool NetworkManager::WasRejected() const
{
	return m_wasRejected;
}

void NetworkManager::ResetAnimationState()
{
	m_animCoordinator.Reset();
	m_animSessionHost.Reset();
	m_localPendingAnimInterest = -1;
	m_pendingAnimInterest.store(-1, std::memory_order_relaxed);
	m_pendingAnimCancel.store(false, std::memory_order_relaxed);
	m_animStateDirty = true;
}

void NetworkManager::BroadcastChangedSessions(const std::vector<uint16_t>& p_changedAnims)
{
	for (uint16_t idx : p_changedAnims) {
		BroadcastAnimUpdate(idx);
	}
	m_animStateDirty = true;
}

void NetworkManager::CancelLocalAnimInterest()
{
	m_animCoordinator.ClearInterest();
	m_localPendingAnimInterest = -1;

	if (IsHost()) {
		HandleAnimCancel(m_localPeerId);
	}
	else if (IsConnected()) {
		AnimCancelMsg msg{};
		msg.header = {MSG_ANIM_CANCEL, m_localPeerId, m_sequence++, TARGET_HOST};
		SendMessage(msg);
	}

	m_animStateDirty = true;
	m_animInterestDirty = true;
}

void NetworkManager::StopAnimation()
{
	ResetAnimationState();
	StopAllPlayback();
}

void NetworkManager::OnWorldEnabled(LegoWorld* p_world)
{
	if (!p_world) {
		return;
	}

	if (p_world->GetWorldId() == LegoOmni::e_act1) {
		m_inIsleWorld = true;
		m_wasInRestrictedArea = IsRestrictedArea(GameState()->m_currentArea);
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
		EnforceDisableNPCs();

		// Refresh animation catalog from the animation manager
		if (AnimationManager()) {
			m_animCatalog.Refresh(AnimationManager());
			m_animCoordinator.SetCatalog(&m_animCatalog);
			m_animSessionHost.SetCatalog(&m_animCatalog);
		}

		m_locationProximity.Reset();
	}
}

void NetworkManager::OnWorldDisabled(LegoWorld* p_world)
{
	if (!p_world) {
		return;
	}

	if (p_world->GetWorldId() == LegoOmni::e_act1) {
		m_inIsleWorld = false;
		m_wasInRestrictedArea = false;
		m_worldSync.SetInIsleWorld(false);

		// Stop animation before ROIs are destroyed (calls ResetAnimationState)
		StopAnimation();
		m_animStateDirty = false; // override: we push explicit empty JSON below
		m_locationProximity.Reset();
		if (m_callbacks) {
			m_callbacks->OnAnimationsAvailable(
				"{\"location\":-1,\"state\":0,\"currentAnimIndex\":65535,\"pendingInterest\":-1,\"animations\":[]}"
			);
		}

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

void NetworkManager::CheckConnectionState()
{
	if (!m_transport || m_connectionState == STATE_DISCONNECTED) {
		return;
	}

	if (m_connectionState == STATE_CONNECTED) {
		if (!m_transport->WasDisconnected()) {
			return;
		}

		if (m_transport->WasRejected()) {
			// Room full on initial connect - flag for game loop to exit
			m_wasRejected = true;
			m_connectionState = STATE_DISCONNECTED;

			if (m_callbacks) {
				m_callbacks->OnConnectionStatusChanged(CONNECTION_STATUS_REJECTED);
			}
			return;
		}

		// Connection lost - enter reconnection
		m_connectionState = STATE_RECONNECTING;
		RemoveAllRemotePlayers();
		m_reconnectAttempt = 0;
		m_reconnectDelay = RECONNECT_INITIAL_DELAY_MS;
		m_nextReconnectTime = SDL_GetTicks() + m_reconnectDelay;

		if (m_callbacks) {
			m_callbacks->OnConnectionStatusChanged(CONNECTION_STATUS_RECONNECTING);
		}
		return;
	}

	// STATE_RECONNECTING
	if (m_transport->IsConnected()) {
		ResetStateAfterReconnect();
		m_connectionState = STATE_CONNECTED;

		if (m_callbacks) {
			m_callbacks->OnConnectionStatusChanged(CONNECTION_STATUS_CONNECTED);
		}
		return;
	}

	uint32_t now = SDL_GetTicks();
	if (now < m_nextReconnectTime) {
		return;
	}

	if (m_reconnectAttempt >= RECONNECT_MAX_ATTEMPTS) {
		// Give up - stay alive but without multiplayer
		m_connectionState = STATE_DISCONNECTED;

		if (m_callbacks) {
			m_callbacks->OnConnectionStatusChanged(CONNECTION_STATUS_FAILED);
		}
		return;
	}

	AttemptReconnect();
}

void NetworkManager::AttemptReconnect()
{
	m_reconnectAttempt++;
	m_transport->Disconnect();
	m_transport->Connect(m_roomId.c_str());
	m_reconnectDelay = SDL_min(m_reconnectDelay * 2, RECONNECT_MAX_DELAY_MS);
	m_nextReconnectTime = SDL_GetTicks() + m_reconnectDelay;
}

void NetworkManager::ResetStateAfterReconnect()
{
	m_localPeerId = 0;
	m_hostPeerId = 0;
	m_sequence = 0;
	m_lastBroadcastTime = 0;
	m_worldSync.ResetForReconnect();
	ResetAnimationState();
}

void NetworkManager::ProcessPendingRequests()
{
	ThirdPersonCamera::Controller* cam = GetCamera();

	// Camera-dependent requests: only consume when cam is available so
	// the request survives until the camera exists.
	if (cam) {
		if (m_pendingToggleThirdPerson.exchange(false, std::memory_order_relaxed)) {
			if (cam->IsEnabled()) {
				if (m_animCoordinator.GetState() != Animation::CoordinationState::e_idle) {
					uint16_t localAnim = m_animCoordinator.GetCurrentAnimIndex();
					CancelLocalAnimInterest();
					if (localAnim != Animation::ANIM_INDEX_NONE) {
						StopScenePlayback(localAnim, false);
					}
				}
				cam->Disable();
				NotifyThirdPersonChanged(false);
			}
			else {
				cam->Enable();
				NotifyThirdPersonChanged(true);
			}
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

	int32_t animInterest = m_pendingAnimInterest.exchange(-1, std::memory_order_relaxed);
	if (animInterest >= 0) {
		// Discard during countdown or playback — player is committed
		Animation::CoordinationState coordState = m_animCoordinator.GetState();
		bool canChangeInterest =
			(coordState == Animation::CoordinationState::e_idle ||
			 coordState == Animation::CoordinationState::e_interested);

		if (canChangeInterest) {
			uint16_t animIndex = static_cast<uint16_t>(animInterest);
			m_animCoordinator.SetInterest(animIndex);
			m_localPendingAnimInterest = animInterest;

			if (IsHost()) {
				uint8_t displayActorIndex = 0;
				ThirdPersonCamera::Controller* animCam = GetCamera();
				if (animCam) {
					displayActorIndex = animCam->GetDisplayActorIndex();
				}
				HandleAnimInterest(m_localPeerId, animIndex, displayActorIndex);

				// If slot assignment failed, clear optimistic interest
				if (!m_animCoordinator.IsLocalPlayerInSession(animIndex)) {
					m_animCoordinator.ClearInterest();
					m_localPendingAnimInterest = -1;
				}
			}
			else if (IsConnected()) {
				AnimInterestMsg msg{};
				msg.header = {MSG_ANIM_INTEREST, m_localPeerId, m_sequence++, TARGET_HOST};
				msg.animIndex = animIndex;
				ThirdPersonCamera::Controller* animCam = GetCamera();
				msg.displayActorIndex = animCam ? animCam->GetDisplayActorIndex() : 0;
				SendMessage(msg);
			}

			m_animStateDirty = true;
			m_animInterestDirty = true;
		}
	}

	if (m_pendingAnimCancel.exchange(false, std::memory_order_relaxed)) {
		CancelLocalAnimInterest();
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

	bool inRestrictedArea = IsRestrictedArea(GameState()->m_currentArea);
	if (m_inIsleWorld && m_wasInRestrictedArea != inRestrictedArea) {
		m_wasInRestrictedArea = inRestrictedArea;
		NotifyPlayerCountChanged();
	}

	PlayerStateMsg msg{};
	msg.header = {MSG_STATE, m_localPeerId, m_sequence++, TARGET_BROADCAST};
	msg.actorId = actorId;
	msg.worldId = inRestrictedArea ? WORLD_NOT_VISIBLE : (int8_t) currentWorld->GetWorldId();
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

		// Zero speed when in any phase of a multi-part emote or animation playback
		if (cam->IsInMultiPartEmote() || cam->IsAnimPlaying()) {
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
				m_animCoordinator.SetLocalPeerId(assignedId);

				LegoAnimationManager::configureLegoAnimationManager(0);
				if (AnimationManager()) {
					AnimationManager()->m_maxAllowedExtras = 0;
					AnimationManager()->m_numAllowedExtras = 0;
				}
				EnforceDisableNPCs();
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
		case MSG_ANIM_INTEREST: {
			AnimInterestMsg msg;
			if (DeserializeMsg(data, length, msg) && msg.header.type == MSG_ANIM_INTEREST) {
				HandleAnimInterest(msg.header.peerId, msg.animIndex, msg.displayActorIndex);
			}
			break;
		}
		case MSG_ANIM_CANCEL: {
			AnimCancelMsg msg;
			if (DeserializeMsg(data, length, msg) && msg.header.type == MSG_ANIM_CANCEL) {
				HandleAnimCancel(msg.header.peerId);
			}
			break;
		}
		case MSG_ANIM_UPDATE: {
			AnimUpdateMsg msg;
			if (DeserializeMsg(data, length, msg) && msg.header.type == MSG_ANIM_UPDATE) {
				HandleAnimUpdate(msg);
			}
			break;
		}
		case MSG_ANIM_START: {
			AnimStartMsg msg;
			if (DeserializeMsg(data, length, msg) && msg.header.type == MSG_ANIM_START) {
				HandleAnimStart(msg);
			}
			break;
		}
		case MSG_ANIM_COMPLETE: {
			AnimCompleteMsg msg;
			if (DeserializeMsg(data, length, msg) && msg.header.type == MSG_ANIM_COMPLETE) {
				HandleAnimComplete(msg);
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
	float radius = m_locationProximity.GetRadius();
	const auto& localLocs = m_locationProximity.GetLocations();
	bool anyInIsle = false;

	for (auto& [peerId, player] : m_remotePlayers) {
		player->Tick(p_deltaTime);

		// Derive locations from remote player's current position
		// Skip players not in the isle world — their position is stale
		if (player->IsSpawned() && player->GetROI() && player->GetWorldId() == (int8_t) LegoOmni::e_act1) {
			anyInIsle = true;

			auto oldLocs = player->GetLocations();
			const float* pos = player->GetROI()->GetWorldPosition();
			auto newLocs = Animation::LocationProximity::ComputeAll(pos[0], pos[2], radius);
			player->SetLocations(std::move(newLocs));
			if (oldLocs != player->GetLocations()) {
				// Dirty if remote's locations changed and any overlap with local player's locations
				for (int16_t loc : localLocs) {
					if (player->IsAtLocation(loc) || std::find(oldLocs.begin(), oldLocs.end(), loc) != oldLocs.end()) {
						m_animStateDirty = true;
						break;
					}
				}
			}
		}
	}

	// Keep pushing while remote players are in the isle world so proximity-based
	// eligibility and session display stay up to date as players move around
	if (anyInIsle) {
		m_animStateDirty = true;
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

		// Send existing session state so the new player sees active sessions
		if (IsHost()) {
			for (const auto& [animIndex, session] : m_animSessionHost.GetSessions()) {
				SendAnimUpdateToPlayer(animIndex, peerId);
			}
		}
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
		m_animStateDirty = true;

		if (IsHost()) {
			std::vector<uint16_t> changedAnims;
			if (m_animSessionHost.HandlePlayerRemoved(peerId, changedAnims)) {
				BroadcastChangedSessions(changedAnims);
			}
		}
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
		m_animStateDirty = true;

		// Player left the isle world — remove from animation sessions
		if (wasInIsle && !nowInIsle && IsHost()) {
			std::vector<uint16_t> changedAnims;
			if (m_animSessionHost.HandlePlayerRemoved(peerId, changedAnims)) {
				BroadcastChangedSessions(changedAnims);
			}
		}
	}
}

void NetworkManager::HandleHostAssign(const HostAssignMsg& p_msg)
{
	uint32_t oldHost = m_hostPeerId;
	m_hostPeerId = p_msg.hostPeerId;

	m_worldSync.SetHost(IsHost());

	if (oldHost != m_hostPeerId) {
		if (!IsHost()) {
			m_worldSync.OnHostChanged();
		}
		// Reset coordination on actual host change, not initial assignment.
		// Initial assignment (oldHost==0) may race with session updates from the host.
		if (oldHost != 0) {
			ResetAnimationState();
		}
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
		const auto& localLocs = m_locationProximity.GetLocations();
		for (int16_t loc : it->second->GetLocations()) {
			if (std::find(localLocs.begin(), localLocs.end(), loc) != localLocs.end()) {
				m_animStateDirty = true;
				break;
			}
		}
		if (it->second->GetROI()) {
			m_roiToPlayer.erase(it->second->GetROI());
		}
		it->second->Despawn();
		m_remotePlayers.erase(it);
		NotifyPlayerCountChanged();

		if (IsHost()) {
			std::vector<uint16_t> changedAnims;
			if (m_animSessionHost.HandlePlayerRemoved(p_peerId, changedAnims)) {
				BroadcastChangedSessions(changedAnims);
			}
		}
	}
}

void NetworkManager::RemoveAllRemotePlayers()
{
	for (auto& [peerId, player] : m_remotePlayers) {
		player->Despawn();
	}
	m_remotePlayers.clear();
	m_roiToPlayer.clear();
	m_animStateDirty = true;
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

		// Only count the local player if they have a valid actor and
		// are not in a restricted overlay area (elevator, observatory, etc.).
		if (!IsRestrictedArea(GameState()->m_currentArea)) {
			LegoPathActor* userActor = UserActor();
			if (userActor && IsValidActorId(static_cast<LegoActor*>(userActor)->GetActorId())) {
				count = 1;
			}
			else if (IsValidActorId(GameState()->GetActorId())) {
				count = 1;
			}
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

void NetworkManager::StopScenePlayback(uint16_t p_animIndex, bool p_unlockRemotes)
{
	auto it = m_playingAnims.find(p_animIndex);
	if (it == m_playingAnims.end()) {
		return;
	}

	// Save before Stop() which resets the flag
	bool wasObserver = it->second->IsObserverMode();

	if (it->second->IsPlaying()) {
		it->second->Stop();
	}

	if (p_unlockRemotes) {
		UnlockRemotesForAnim(p_animIndex);
	}

	// Release camera if local player was a participant (not observer) in this animation
	if (!wasObserver) {
		ThirdPersonCamera::Controller* cam = GetCamera();
		if (cam) {
			cam->SetAnimPlaying(false);
		}
	}

	m_playingAnims.erase(it);
}

void NetworkManager::StopAllPlayback()
{
	for (auto& [animIndex, scenePlayer] : m_playingAnims) {
		if (scenePlayer->IsPlaying()) {
			scenePlayer->Stop();
		}
	}
	m_playingAnims.clear();

	for (auto& [peerId, player] : m_remotePlayers) {
		player->ForceUnlockAnimation();
	}

	ThirdPersonCamera::Controller* cam = GetCamera();
	if (cam) {
		cam->SetAnimPlaying(false);
	}
}

void NetworkManager::UnlockRemotesForAnim(uint16_t p_animIndex)
{
	for (auto& [peerId, player] : m_remotePlayers) {
		player->UnlockFromAnimation(p_animIndex);
	}
}

void NetworkManager::TickAnimation()
{
	if (m_playingAnims.empty()) {
		return;
	}

	// Collect completed animations with their observer mode (Tick/Stop resets the flag)
	std::vector<std::pair<uint16_t, bool>> completed;

	for (auto& [animIndex, scenePlayer] : m_playingAnims) {
		if (!scenePlayer->IsPlaying()) {
			completed.push_back({animIndex, scenePlayer->IsObserverMode()});
			continue;
		}

		bool wasObserver = scenePlayer->IsObserverMode();
		scenePlayer->Tick();

		if (!scenePlayer->IsPlaying()) {
			completed.push_back({animIndex, wasObserver});
		}
	}

	for (auto& [animIndex, wasObserver] : completed) {
		UnlockRemotesForAnim(animIndex);

		// Release camera if local player was a participant (not observer)
		if (!wasObserver) {
			ThirdPersonCamera::Controller* cam = GetCamera();
			if (cam) {
				cam->SetAnimPlaying(false);
			}
			m_animCoordinator.ResetLocalState();
			m_animCoordinator.RemoveSession(animIndex);
		}

		if (IsHost()) {
			BroadcastAnimComplete(animIndex); // Must fire before EraseSession destroys participant data
			m_animSessionHost.EraseSession(animIndex);
			BroadcastAnimUpdate(animIndex); // Broadcast cleared state
		}

		m_playingAnims.erase(animIndex);
		m_animStateDirty = true;
		m_animInterestDirty = true;
	}
}

void NetworkManager::TickHostSessions()
{
	// Check co-location for all sessions: start/revert countdown as needed.
	// For cam anims, also auto-remove players who left the required location.
	// Use a snapshot of keys since we may modify sessions during iteration.
	std::vector<uint16_t> sessionKeys;
	for (const auto& [animIndex, session] : m_animSessionHost.GetSessions()) {
		sessionKeys.push_back(animIndex);
	}

	for (uint16_t animIndex : sessionKeys) {
		const Animation::AnimSession* session = m_animSessionHost.FindSession(animIndex);
		if (!session || session->state == Animation::CoordinationState::e_playing) {
			continue;
		}

		// For cam anims: auto-remove players who left the required location
		const Animation::CatalogEntry* entry = m_animCatalog.FindEntry(animIndex);
		if (entry && entry->location >= 0) {
			std::vector<uint32_t> toRemove;
			for (const auto& slot : session->slots) {
				if (slot.peerId != 0 && !IsPeerAtLocation(slot.peerId, entry->location)) {
					toRemove.push_back(slot.peerId);
				}
			}
			for (uint32_t pid : toRemove) {
				std::vector<uint16_t> changed;
				m_animSessionHost.HandleCancel(pid, changed);
				BroadcastChangedSessions(changed);
			}
			session = m_animSessionHost.FindSession(animIndex);
			if (!session) {
				continue;
			}
		}

		// Auto-remove participants whose vehicle state no longer matches
		if (entry && entry->vehicleMask) {
			std::vector<uint32_t> toRemove;
			for (const auto& slot : session->slots) {
				if (slot.peerId != 0 && !slot.IsSpectator()) {
					int8_t charIdx = slot.charIndex;
					uint8_t onVehicle = GetPeerVehicleState(slot.peerId, charIdx);
					if (!Animation::Catalog::CheckVehicleEligibility(entry, charIdx, onVehicle)) {
						toRemove.push_back(slot.peerId);
					}
				}
			}
			for (uint32_t pid : toRemove) {
				std::vector<uint16_t> changed;
				m_animSessionHost.HandleCancel(pid, changed);
				BroadcastChangedSessions(changed);
			}
			session = m_animSessionHost.FindSession(animIndex);
			if (!session) {
				continue;
			}
		}

		bool allFilled = m_animSessionHost.AreAllSlotsFilled(animIndex);
		bool coLocated = allFilled && ValidateSessionLocations(animIndex);

		if (session->state == Animation::CoordinationState::e_interested && coLocated) {
			m_animSessionHost.StartCountdown(animIndex);

			if (m_animCoordinator.IsLocalPlayerInSession(animIndex)) {
				const AnimInfo* ai = m_animCatalog.GetAnimInfo(animIndex);
				if (ai) {
					m_animLoader.PreloadAsync(ai->m_objectId);
				}
			}

			BroadcastAnimUpdate(animIndex);
			m_animStateDirty = true;
		}
		else if (session->state == Animation::CoordinationState::e_countdown && !coLocated) {
			m_animSessionHost.RevertCountdown(animIndex);
			BroadcastAnimUpdate(animIndex);
			m_animStateDirty = true;
		}
	}

	// Check countdown expiry — multiple animations may be ready simultaneously
	std::vector<uint16_t> readyAnims = m_animSessionHost.Tick(SDL_GetTicks());
	for (uint16_t readyAnim : readyAnims) {
		BroadcastAnimStart(readyAnim);
		HandleAnimStartLocally(readyAnim, m_animCoordinator.IsLocalPlayerInSession(readyAnim));
	}

	// During countdown, push state every tick so countdownMs reaches the frontend
	if (m_animSessionHost.HasCountdownSession()) {
		m_animStateDirty = true;
	}
}

void NetworkManager::HandleAnimInterest(uint32_t p_peerId, uint16_t p_animIndex, uint8_t p_displayActorIndex)
{
	if (!IsHost()) {
		return;
	}

	// For location-bound animations, player must be at that location
	const Animation::CatalogEntry* entry = m_animCatalog.FindEntry(p_animIndex);
	if (entry && entry->location >= 0) {
		if (!IsPeerAtLocation(p_peerId, entry->location)) {
			return;
		}
	}

	// Validate vehicle eligibility if the joining player would be a performer
	if (entry) {
		int8_t charIndex = Animation::Catalog::DisplayActorToCharacterIndex(p_displayActorIndex);
		if ((entry->performerMask >> charIndex) & 1) {
			uint8_t onVehicle = GetPeerVehicleState(p_peerId, charIndex);
			if (!Animation::Catalog::CheckVehicleEligibility(entry, charIndex, onVehicle)) {
				return;
			}
		}
	}

	// For NPC anims: if all slots are full, remove far-away participants to make room
	// for the new nearby player. This only fires when slots are exhausted — if there's
	// an open slot, the new player just joins normally without disturbing anyone.
	if (entry && entry->location == -1 && m_animSessionHost.AreAllSlotsFilled(p_animIndex)) {
		float newX, newZ;
		if (GetPeerPosition(p_peerId, newX, newZ)) {
			const Animation::AnimSession* session = m_animSessionHost.FindSession(p_animIndex);
			if (session) {
				std::vector<uint32_t> stale;
				for (const auto& slot : session->slots) {
					if (slot.peerId != 0 && slot.peerId != p_peerId && !IsPeerNearby(slot.peerId, newX, newZ)) {
						stale.push_back(slot.peerId);
					}
				}
				for (uint32_t pid : stale) {
					std::vector<uint16_t> changed;
					m_animSessionHost.HandleCancel(pid, changed);
					BroadcastChangedSessions(changed);
				}
			}
		}
	}

	std::vector<uint16_t> changedAnims;
	if (m_animSessionHost.HandleInterest(p_peerId, p_animIndex, p_displayActorIndex, changedAnims)) {
		BroadcastChangedSessions(changedAnims);
		m_animInterestDirty = true;
	}
}

void NetworkManager::HandleAnimCancel(uint32_t p_peerId)
{
	if (!IsHost()) {
		return;
	}

	uint16_t localAnimBefore = m_animCoordinator.GetCurrentAnimIndex();
	Animation::CoordinationState oldState = m_animCoordinator.GetState();

	std::vector<uint16_t> changedAnims;
	if (m_animSessionHost.HandleCancel(p_peerId, changedAnims)) {
		BroadcastChangedSessions(changedAnims);
		m_animInterestDirty = true;
	}

	// Stop local player's animation if their session was erased
	if (oldState == Animation::CoordinationState::e_playing &&
		m_animCoordinator.GetState() == Animation::CoordinationState::e_idle &&
		localAnimBefore != Animation::ANIM_INDEX_NONE) {
		StopScenePlayback(localAnimBefore, true);
	}

	// Stop observer-mode playback for any erased playing sessions
	for (uint16_t animIndex : changedAnims) {
		if (animIndex != localAnimBefore && m_playingAnims.count(animIndex)) {
			StopScenePlayback(animIndex, true);
		}
	}
}

void NetworkManager::HandleAnimUpdate(const AnimUpdateMsg& p_msg)
{
	if (IsHost()) {
		return; // Host already updated its own state
	}

	uint16_t localAnimBefore = m_animCoordinator.GetCurrentAnimIndex();
	Animation::CoordinationState oldState = m_animCoordinator.GetState();

	uint32_t slots[8];
	ExtractSlotPeerIds(p_msg, slots);

	m_animCoordinator.ApplySessionUpdate(p_msg.animIndex, p_msg.state, p_msg.countdownMs, slots, p_msg.slotCount);

	if (p_msg.state == static_cast<uint8_t>(Animation::CoordinationState::e_countdown)) {
		const AnimInfo* ai = m_animCatalog.GetAnimInfo(p_msg.animIndex);
		if (ai) {
			m_animLoader.PreloadAsync(ai->m_objectId);
		}
	}

	// If local player's pending interest matches, clear it (host has responded)
	if (m_localPendingAnimInterest >= 0 && static_cast<uint16_t>(m_localPendingAnimInterest) == p_msg.animIndex) {
		m_localPendingAnimInterest = -1;
	}

	// Stop local player's animation if their session was cleared
	if (oldState == Animation::CoordinationState::e_playing &&
		m_animCoordinator.GetState() == Animation::CoordinationState::e_idle &&
		localAnimBefore != Animation::ANIM_INDEX_NONE) {
		StopScenePlayback(localAnimBefore, true);
	}

	// Stop observer playback when the observed session is cleared
	if (m_playingAnims.count(p_msg.animIndex) && p_msg.state == 0) {
		StopScenePlayback(p_msg.animIndex, true);
	}

	m_animStateDirty = true;
	m_animInterestDirty = true;
}

void NetworkManager::HandleAnimStart(const AnimStartMsg& p_msg)
{
	if (IsHost()) {
		return; // Host handles locally in BroadcastAnimStart
	}

	m_animCoordinator.ApplyAnimStart(p_msg.animIndex);
	HandleAnimStartLocally(p_msg.animIndex, m_animCoordinator.IsLocalPlayerInSession(p_msg.animIndex));

	m_animStateDirty = true;
	m_animInterestDirty = true;
}

void NetworkManager::HandleAnimStartLocally(uint16_t p_animIndex, bool p_localInSession)
{
	auto abortSession = [&]() {
		// Observers must not abort the authoritative session — only participants may do that
		if (p_localInSession) {
			if (IsHost()) {
				m_animSessionHost.EraseSession(p_animIndex);
				BroadcastAnimUpdate(p_animIndex);
			}
			m_animCoordinator.ResetLocalState();
		}
		m_animStateDirty = true;
	};

	const AnimInfo* animInfo = m_animCatalog.GetAnimInfo(p_animIndex);
	if (!animInfo) {
		abortSession();
		return;
	}

	const Animation::CatalogEntry* entry = m_animCatalog.FindEntry(p_animIndex);
	if (!entry) {
		abortSession();
		return;
	}

	ThirdPersonCamera::Controller* cam = GetCamera();
	if (p_localInSession && (!cam || !cam->GetDisplayROI())) {
		abortSession();
		return;
	}

	const Animation::SessionView* view = m_animCoordinator.GetSessionView(p_animIndex);
	std::vector<int8_t> slotChars = Animation::SessionHost::ComputeSlotCharIndices(entry);

	bool observerMode = !p_localInSession;

	// Build participants: local player first (if participating), then remotes
	int8_t localCharIndex = -1;
	std::vector<Animation::ParticipantROI> participants;

	if (view) {
		uint8_t count = view->slotCount < (uint8_t) slotChars.size() ? view->slotCount : (uint8_t) slotChars.size();
		for (uint8_t i = 0; i < count; i++) {
			uint32_t peerId = view->peerSlots[i];
			if (peerId == 0) {
				continue;
			}

			if (peerId == m_localPeerId) {
				localCharIndex = slotChars[i];
				continue;
			}

			auto it = m_remotePlayers.find(peerId);
			if (it == m_remotePlayers.end() || !it->second->GetROI()) {
				continue;
			}

			Animation::ParticipantROI rp;
			rp.roi = it->second->GetROI();
			rp.vehicleROI = it->second->GetRideVehicleROI();
			rp.charIndex = slotChars[i];
			participants.push_back(rp);

			// Lock performers to prevent network updates from fighting animation
			if (!rp.IsSpectator()) {
				it->second->LockForAnimation(p_animIndex);
			}
		}
	}

	// Insert local player at index 0 only when participating
	if (!observerMode) {
		Animation::ParticipantROI local;
		local.roi = cam->GetDisplayROI();
		local.vehicleROI = cam->GetRideVehicleROI();
		local.charIndex = localCharIndex;
		participants.insert(participants.begin(), local);
	}

	if (participants.empty()) {
		abortSession();
		return;
	}

	auto scenePlayer = std::make_unique<Animation::ScenePlayer>();
	scenePlayer->SetLoader(&m_animLoader);

	if (!observerMode) {
		bool localIsPerformer = (localCharIndex >= 0);
		cam->SetAnimPlaying(true, localIsPerformer, [this, p_animIndex]() {
			auto it = m_playingAnims.find(p_animIndex);
			if (it != m_playingAnims.end()) {
				it->second->Stop();
			}
		});
	}

	scenePlayer->Play(animInfo, entry->category, participants.data(), (uint8_t) participants.size(), observerMode);

	if (!scenePlayer->IsPlaying()) {
		if (!observerMode) {
			cam->SetAnimPlaying(false);
		}
		UnlockRemotesForAnim(p_animIndex);
		abortSession();
		return;
	}

	m_playingAnims[p_animIndex] = std::move(scenePlayer);
	m_localPendingAnimInterest = -1;
	m_animStateDirty = true;
}

AnimUpdateMsg NetworkManager::BuildAnimUpdateMsg(uint16_t p_animIndex, uint32_t p_target)
{
	AnimUpdateMsg msg{};
	msg.header = {MSG_ANIM_UPDATE, m_localPeerId, m_sequence++, p_target};
	msg.animIndex = p_animIndex;

	const Animation::AnimSession* session = m_animSessionHost.FindSession(p_animIndex);
	if (session) {
		msg.state = static_cast<uint8_t>(session->state);
		msg.countdownMs = Animation::SessionHost::ComputeCountdownMs(*session, SDL_GetTicks());
		msg.slotCount = static_cast<uint8_t>(session->slots.size() < 8 ? session->slots.size() : 8);
		for (uint8_t i = 0; i < msg.slotCount; i++) {
			msg.slots[i].peerId = session->slots[i].peerId;
		}
	}
	// else: zero-initialized = cleared state
	return msg;
}

void NetworkManager::BroadcastAnimUpdate(uint16_t p_animIndex)
{
	AnimUpdateMsg msg = BuildAnimUpdateMsg(p_animIndex, TARGET_BROADCAST);
	SendMessage(msg);

	// Also update local coordinator
	uint32_t slots[8];
	ExtractSlotPeerIds(msg, slots);
	m_animCoordinator.ApplySessionUpdate(msg.animIndex, msg.state, msg.countdownMs, slots, msg.slotCount);
}

void NetworkManager::SendAnimUpdateToPlayer(uint16_t p_animIndex, uint32_t p_targetPeerId)
{
	SendMessage(BuildAnimUpdateMsg(p_animIndex, p_targetPeerId));
}

void NetworkManager::BroadcastAnimStart(uint16_t p_animIndex)
{
	AnimStartMsg msg{};
	msg.header = {MSG_ANIM_START, m_localPeerId, m_sequence++, TARGET_BROADCAST};
	msg.animIndex = p_animIndex;
	SendMessage(msg);

	// Also update local coordinator
	m_animCoordinator.ApplyAnimStart(p_animIndex);
}

void NetworkManager::BroadcastAnimComplete(uint16_t p_animIndex)
{
	const Animation::AnimSession* session = m_animSessionHost.FindSession(p_animIndex);
	if (!session) {
		return;
	}

	const AnimInfo* animInfo = m_animCatalog.GetAnimInfo(p_animIndex);
	if (!animInfo) {
		return;
	}

	AnimCompleteMsg msg{};
	msg.header = {MSG_ANIM_COMPLETE, m_localPeerId, m_sequence++, TARGET_BROADCAST};
	msg.eventId = (static_cast<uint64_t>(SDL_rand_bits()) << 32) | static_cast<uint64_t>(SDL_rand_bits());
	msg.objectId = animInfo->m_objectId;
	msg.participantCount = 0;

	char localName[8];
	EncodeUsername(localName);

	for (const auto& slot : session->slots) {
		if (slot.peerId == 0 || msg.participantCount >= 8) {
			continue;
		}

		AnimCompletionParticipant& p = msg.participants[msg.participantCount];
		p.peerId = slot.peerId;

		if (slot.IsSpectator()) {
			// Resolve spectator's actual character from their display actor
			if (slot.peerId == m_localPeerId) {
				ThirdPersonCamera::Controller* cam = GetCamera();
				p.charIndex = cam ? Animation::Catalog::DisplayActorToCharacterIndex(cam->GetDisplayActorIndex()) : -1;
			}
			else {
				auto it = m_remotePlayers.find(slot.peerId);
				p.charIndex = it != m_remotePlayers.end()
								  ? Animation::Catalog::DisplayActorToCharacterIndex(it->second->GetDisplayActorIndex())
								  : -1;
			}
		}
		else {
			p.charIndex = slot.charIndex;
		}

		if (slot.peerId == m_localPeerId) {
			SDL_memcpy(p.displayName, localName, sizeof(p.displayName));
		}
		else {
			auto it = m_remotePlayers.find(slot.peerId);
			if (it != m_remotePlayers.end()) {
				SDL_memcpy(p.displayName, it->second->GetDisplayName(), sizeof(p.displayName));
			}
			else {
				p.displayName[0] = '\0';
			}
		}

		msg.participantCount++;
	}

	SendMessage(msg);

	// Also handle locally on the host (message sent to TARGET_BROADCAST excludes sender)
	HandleAnimComplete(msg);
}

void NetworkManager::HandleAnimComplete(const AnimCompleteMsg& p_msg)
{
	// Only fire callback for actual participants, not observers
	int localIdx = -1;
	for (uint8_t i = 0; i < p_msg.participantCount; i++) {
		if (p_msg.participants[i].peerId == m_localPeerId) {
			localIdx = i;
			break;
		}
	}

	if (localIdx < 0 || !m_callbacks) {
		return;
	}

	// Build JSON for frontend
	char eventIdHex[17];
	SDL_snprintf(
		eventIdHex,
		sizeof(eventIdHex),
		"%08x%08x",
		static_cast<uint32_t>(p_msg.eventId >> 32),
		static_cast<uint32_t>(p_msg.eventId & 0xFFFFFFFF)
	);

	std::string json = "{\"eventId\":\"";
	json += eventIdHex;
	json += "\",\"objectId\":";
	json += std::to_string(p_msg.objectId);
	json += ",\"participants\":[";

	// Emit local player first so frontend can rely on participants[0] being self
	bool first = true;
	auto appendParticipant = [&](uint8_t i) {
		if (!first) {
			json += ',';
		}
		first = false;
		const AnimCompletionParticipant& p = p_msg.participants[i];
		// Ensure null-termination safety for displayName (protocol uses fixed char[8])
		char name[8];
		SDL_memcpy(name, p.displayName, sizeof(name));
		name[7] = '\0';
		json += "{\"charIndex\":";
		json += std::to_string(static_cast<int>(p.charIndex));
		json += ",\"displayName\":\"";
		json += name;
		json += "\"}";
	};

	appendParticipant(static_cast<uint8_t>(localIdx));
	for (uint8_t i = 0; i < p_msg.participantCount; i++) {
		if (i != static_cast<uint8_t>(localIdx)) {
			appendParticipant(i);
		}
	}

	json += "]}";

	m_callbacks->OnAnimationCompleted(json.c_str());
}

bool NetworkManager::IsPeerAtLocation(uint32_t p_peerId, int16_t p_location) const
{
	if (p_peerId == m_localPeerId) {
		return m_locationProximity.IsAtLocation(p_location);
	}
	auto it = m_remotePlayers.find(p_peerId);
	if (it != m_remotePlayers.end()) {
		return it->second->IsAtLocation(p_location);
	}
	return false;
}

bool NetworkManager::GetPeerPosition(uint32_t p_peerId, float& p_x, float& p_z) const
{
	if (p_peerId == m_localPeerId) {
		LegoPathActor* userActor = UserActor();
		if (userActor && userActor->GetROI()) {
			const float* pos = userActor->GetROI()->GetWorldPosition();
			p_x = pos[0];
			p_z = pos[2];
			return true;
		}
		return false;
	}
	auto it = m_remotePlayers.find(p_peerId);
	if (it != m_remotePlayers.end() && it->second->IsSpawned() && it->second->GetROI()) {
		const float* pos = it->second->GetROI()->GetWorldPosition();
		p_x = pos[0];
		p_z = pos[2];
		return true;
	}
	return false;
}

bool NetworkManager::IsPeerNearby(uint32_t p_peerId, float p_refX, float p_refZ) const
{
	if (p_peerId == 0) {
		return false;
	}
	if (p_peerId == m_localPeerId) {
		return true;
	}
	auto it = m_remotePlayers.find(p_peerId);
	if (it == m_remotePlayers.end() || !it->second->IsSpawned() || !it->second->GetROI() ||
		it->second->GetWorldId() != (int8_t) LegoOmni::e_act1) {
		return false;
	}
	const float* pos = it->second->GetROI()->GetWorldPosition();
	float dx = pos[0] - p_refX;
	float dz = pos[2] - p_refZ;
	return (dx * dx + dz * dz) <= NPC_ANIM_NEARBY_RADIUS_SQ;
}

uint8_t NetworkManager::GetPeerVehicleState(uint32_t p_peerId, int8_t p_charIndex) const
{
	if (p_peerId == m_localPeerId) {
		ThirdPersonCamera::Controller* cam = GetCamera();
		return cam ? Animation::Catalog::GetVehicleState(p_charIndex, cam->GetRideVehicleROI())
				   : Animation::Catalog::e_onFoot;
	}
	auto it = m_remotePlayers.find(p_peerId);
	if (it == m_remotePlayers.end() || !it->second->IsSpawned()) {
		return Animation::Catalog::e_onFoot;
	}
	return Animation::Catalog::GetVehicleState(p_charIndex, it->second->GetRideVehicleROI());
}

bool NetworkManager::ValidateSessionLocations(uint16_t p_animIndex)
{
	const Animation::AnimSession* session = m_animSessionHost.FindSession(p_animIndex);
	if (!session) {
		return false;
	}

	const Animation::CatalogEntry* entry = m_animCatalog.FindEntry(p_animIndex);
	if (!entry) {
		return false;
	}

	if (entry->location >= 0) {
		// Cam anim: all participants must be at the specific location
		for (const auto& slot : session->slots) {
			if (slot.peerId == 0) {
				continue;
			}
			if (!IsPeerAtLocation(slot.peerId, entry->location)) {
				return false;
			}
		}
		return true;
	}

	// NPC anim: all participants must be within NPC_ANIM_PROXIMITY of each other
	float firstX = 0, firstZ = 0;
	bool hasFirst = false;

	for (const auto& slot : session->slots) {
		if (slot.peerId == 0) {
			continue;
		}

		float px, pz;
		if (!GetPeerPosition(slot.peerId, px, pz)) {
			continue; // Position unknown — don't block
		}

		if (!hasFirst) {
			firstX = px;
			firstZ = pz;
			hasFirst = true;
		}
		else {
			float dx = px - firstX;
			float dz = pz - firstZ;
			if ((dx * dx + dz * dz) > (Animation::NPC_ANIM_PROXIMITY * Animation::NPC_ANIM_PROXIMITY)) {
				return false;
			}
		}
	}

	return true;
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
			if (!it->second->IsMoving() && !it->second->IsInMultiPartEmote() && !it->second->IsAnimationLocked()) {
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

			// Only play click animation in 3rd person (not during multi-part emote or animation playback)
			if (cam->GetDisplayROI() && !cam->IsInVehicle() && !cam->IsInMultiPartEmote() && !cam->IsAnimPlaying()) {
				cam->StopClickAnimation();
				MxU32 clickAnimId =
					Common::CharacterCustomizer::PlayClickAnimation(cam->GetDisplayROI(), cam->GetCustomizeState());
				cam->SetClickAnimObjectId(clickAnimId);
			}
		}
	}
}

// Helper: append a JSON-escaped string value (assumes no control chars in input)
static void JsonAppendString(std::string& p_out, const char* p_str)
{
	p_out += '"';
	p_out += p_str;
	p_out += '"';
}

static void BuildAnimationJson(
	std::string& p_json,
	const Animation::EligibilityInfo& p_info,
	const AnimInfo* p_animInfo,
	uint8_t p_sessionState,
	uint16_t p_countdownMs,
	bool p_localInSession,
	int8_t p_localCharIndex
)
{
	p_json += "{\"animIndex\":";
	p_json += std::to_string(p_info.animIndex);
	p_json += ",\"name\":";
	JsonAppendString(p_json, p_animInfo->m_name ? p_animInfo->m_name : "");
	p_json += ",\"objectId\":";
	p_json += std::to_string(p_animInfo->m_objectId);
	p_json += ",\"category\":";
	p_json += std::to_string(static_cast<uint8_t>(p_info.entry->category));
	p_json += ",\"eligible\":";
	p_json += p_info.eligible ? "true" : "false";
	p_json += ",\"atLocation\":";
	p_json += p_info.atLocation ? "true" : "false";
	p_json += ",\"sessionState\":";
	p_json += std::to_string(p_sessionState);
	p_json += ",\"countdownMs\":";
	p_json += std::to_string(p_countdownMs);
	p_json += ",\"localInSession\":";
	p_json += p_localInSession ? "true" : "false";

	// canJoin: local player could fill an unfilled slot (checked via bitmasks)
	bool canJoin = false;
	if (!p_localInSession && p_sessionState >= 1 && p_localCharIndex >= 0) {
		uint64_t localBit = uint64_t(1) << p_localCharIndex;
		if ((p_info.entry->performerMask & localBit)) {
			// Find this performer's slot index and check if unfilled
			uint8_t slotIdx = 0;
			for (int8_t bit = 0; bit < p_localCharIndex; bit++) {
				if (p_info.entry->performerMask & (uint64_t(1) << bit)) {
					slotIdx++;
				}
			}
			if (slotIdx < p_info.slots.size() && !p_info.slots[slotIdx].filled) {
				canJoin = true;
			}
		}
		else {
			// Check spectator slot (last slot): unfilled and player is eligible
			if (!p_info.slots.empty() && !p_info.slots.back().filled &&
				Animation::Catalog::CanParticipateChar(p_info.entry, p_localCharIndex)) {
				canJoin = true;
			}
		}
	}
	p_json += ",\"canJoin\":";
	p_json += canJoin ? "true" : "false";

	p_json += ",\"slots\":[";
	for (size_t s = 0; s < p_info.slots.size(); s++) {
		const auto& slot = p_info.slots[s];
		if (s > 0) {
			p_json += ',';
		}
		p_json += "{\"names\":[";
		for (size_t n = 0; n < slot.names.size(); n++) {
			if (n > 0) {
				p_json += ',';
			}
			JsonAppendString(p_json, slot.names[n]);
		}
		p_json += "],\"filled\":";
		p_json += slot.filled ? "true" : "false";
		p_json += '}';
	}
	p_json += "]}";
}

void NetworkManager::PushAnimationState()
{
	ThirdPersonCamera::Controller* cam = GetCamera();
	if (!cam || !cam->GetDisplayROI()) {
		// Camera unavailable — push idle state so the frontend clears any countdown/session UI
		if (m_callbacks) {
			m_callbacks->OnAnimationsAvailable(IDLE_ANIM_STATE_JSON);
		}
		return;
	}

	const auto& locations = m_locationProximity.GetLocations();
	uint8_t displayActorIndex = cam->GetDisplayActorIndex();
	int8_t localCharIndex = Animation::Catalog::DisplayActorToCharacterIndex(displayActorIndex);

	LegoPathActor* userActor = UserActor();
	if (!userActor || !userActor->GetROI()) {
		if (m_callbacks) {
			m_callbacks->OnAnimationsAvailable(IDLE_ANIM_STATE_JSON);
		}
		return;
	}
	const float* localPos = userActor->GetROI()->GetWorldPosition();
	float localX = localPos[0], localZ = localPos[2];

	// Build proximity character indices and vehicle state (for NPC anims — position-based, not location-based)
	std::vector<int8_t> proximityCharIndices;
	std::vector<uint8_t> proximityVehicleState;
	proximityCharIndices.push_back(localCharIndex);
	proximityVehicleState.push_back(Animation::Catalog::GetVehicleState(localCharIndex, cam->GetRideVehicleROI()));

	for (const auto& [peerId, player] : m_remotePlayers) {
		if (!player->IsSpawned() || !player->GetROI() || player->GetWorldId() != (int8_t) LegoOmni::e_act1) {
			continue;
		}
		// Exact NPC_ANIM_PROXIMITY radius for triggering eligibility
		// (tighter than IsPeerNearby's NPC_ANIM_NEARBY_RADIUS_SQ used for session visibility)
		const float* rpos = player->GetROI()->GetWorldPosition();
		float dx = rpos[0] - localX;
		float dz = rpos[2] - localZ;
		if ((dx * dx + dz * dz) <= (Animation::NPC_ANIM_PROXIMITY * Animation::NPC_ANIM_PROXIMITY)) {
			int8_t charIdx = Animation::Catalog::DisplayActorToCharacterIndex(player->GetDisplayActorIndex());
			proximityCharIndices.push_back(charIdx);
			proximityVehicleState.push_back(Animation::Catalog::GetVehicleState(charIdx, player->GetRideVehicleROI()));
		}
	}

	// Compute eligibility across all overlapping locations.
	// Each call returns NPC anims + cam anims for that specific location.
	// NPC anims are identical across calls (same proximityChars), so we deduplicate by animIndex.
	std::vector<Animation::EligibilityInfo> eligibility;
	std::set<uint16_t> seenAnimIndices;

	// If at no location, still process once with -1 to get NPC anims
	std::vector<int16_t> locationsToProcess = locations.empty() ? std::vector<int16_t>{int16_t(-1)} : locations;

	for (int16_t loc : locationsToProcess) {
		// Build per-location character indices and vehicle state (for cam anims at this location)
		std::vector<int8_t> locationCharIndices;
		std::vector<uint8_t> locationVehicleState;
		locationCharIndices.push_back(localCharIndex);
		locationVehicleState.push_back(Animation::Catalog::GetVehicleState(localCharIndex, cam->GetRideVehicleROI()));

		for (const auto& [peerId, player] : m_remotePlayers) {
			if (!player->IsSpawned() || !player->GetROI() || player->GetWorldId() != (int8_t) LegoOmni::e_act1) {
				continue;
			}
			if (player->IsAtLocation(loc)) {
				int8_t charIdx = Animation::Catalog::DisplayActorToCharacterIndex(player->GetDisplayActorIndex());
				locationCharIndices.push_back(charIdx);
				locationVehicleState.push_back(Animation::Catalog::GetVehicleState(charIdx, player->GetRideVehicleROI()));
			}
		}

		auto locEligibility = m_animCoordinator.ComputeEligibility(
			loc,
			locationCharIndices.data(),
			locationVehicleState.data(),
			static_cast<uint8_t>(locationCharIndices.size()),
			proximityCharIndices.data(),
			proximityVehicleState.data(),
			static_cast<uint8_t>(proximityCharIndices.size())
		);

		for (auto& info : locEligibility) {
			if (seenAnimIndices.insert(info.animIndex).second) {
				eligibility.push_back(std::move(info));
			}
		}
	}

	// TODO(vehicle-filter): Remove this logging block after verification
	if (m_vehicleFilterLogPending) {
		m_vehicleFilterLogPending = false;
		uint8_t vehState = Animation::Catalog::GetVehicleState(localCharIndex, cam->GetRideVehicleROI());
		const char* stateNames[] = {"onFoot", "onOwnVehicle", "onOtherVehicle"};
		SDL_Log(
			"[VehicleFilter] Vehicle state changed: char=%d state=%s — %zu eligible animations",
			localCharIndex,
			stateNames[vehState < 3 ? vehState : 0],
			eligibility.size()
		);
		uint32_t vehicleAnimCount = 0;
		for (const auto& info : eligibility) {
			const AnimInfo* ai = m_animCatalog.GetAnimInfo(info.animIndex);
			if (ai && info.entry && info.entry->vehicleMask) {
				vehicleAnimCount++;
				SDL_Log(
					"  [%u] %s (objId=%u loc=%d vmask=0x%02x)",
					info.animIndex,
					ai->m_name,
					ai->m_objectId,
					ai->m_location,
					info.entry->vehicleMask
				);
			}
		}
		if (vehicleAnimCount == 0) {
			SDL_Log("  (no vehicle animations in eligible set)");
		}
	}

	// Build JSON
	std::string json;
	json.reserve(2048);
	json += "{\"locations\":[";
	for (size_t i = 0; i < locations.size(); i++) {
		if (i > 0) {
			json += ',';
		}
		json += std::to_string(locations[i]);
	}
	json += "],\"state\":";
	json += std::to_string(static_cast<uint8_t>(m_animCoordinator.GetState()));
	json += ",\"currentAnimIndex\":";
	json += std::to_string(m_animCoordinator.GetCurrentAnimIndex());
	json += ",\"pendingInterest\":";
	json += std::to_string(m_localPendingAnimInterest);
	json += ",\"animations\":[";

	bool firstAnim = true;
	for (size_t i = 0; i < eligibility.size(); i++) {
		const auto& info = eligibility[i];
		const AnimInfo* animInfo = m_animCatalog.GetAnimInfo(info.animIndex);
		if (!animInfo) {
			continue;
		}

		if (!firstAnim) {
			json += ',';
		}
		firstAnim = false;

		// Session state: host computes live countdown, clients derive from countdownEndTime
		uint8_t sessionState = 0;
		uint16_t countdownMs = 0;
		if (IsHost()) {
			const Animation::AnimSession* hostSession = m_animSessionHost.FindSession(info.animIndex);
			if (hostSession) {
				sessionState = static_cast<uint8_t>(hostSession->state);
				countdownMs = Animation::SessionHost::ComputeCountdownMs(*hostSession, SDL_GetTicks());
			}
		}
		else {
			const Animation::SessionView* sv = m_animCoordinator.GetSessionView(info.animIndex);
			if (sv) {
				sessionState = static_cast<uint8_t>(sv->state);
				if (sv->state == Animation::CoordinationState::e_countdown && sv->countdownEndTime > 0) {
					uint32_t now = SDL_GetTicks();
					countdownMs = (now < sv->countdownEndTime) ? static_cast<uint16_t>(sv->countdownEndTime - now) : 0;
				}
				else {
					countdownMs = sv->countdownMs;
				}
			}
		}

		bool localInSession = m_animCoordinator.IsLocalPlayerInSession(info.animIndex);

		// Suppress session display if local player is not in the session and no
		// session participant is nearby — prevents stale "Join!" for far-away sessions
		if (sessionState > 0 && !localInSession) {
			bool anyParticipantNearby = false;

			if (IsHost()) {
				const Animation::AnimSession* hs = m_animSessionHost.FindSession(info.animIndex);
				if (hs) {
					for (const auto& slot : hs->slots) {
						if (IsPeerNearby(slot.peerId, localX, localZ)) {
							anyParticipantNearby = true;
							break;
						}
					}
				}
			}
			else {
				const Animation::SessionView* ssv = m_animCoordinator.GetSessionView(info.animIndex);
				if (ssv) {
					for (uint8_t s = 0; s < ssv->slotCount && !anyParticipantNearby; s++) {
						if (IsPeerNearby(ssv->peerSlots[s], localX, localZ)) {
							anyParticipantNearby = true;
						}
					}
				}
			}

			if (!anyParticipantNearby) {
				sessionState = 0;
				countdownMs = 0;
			}
		}

		BuildAnimationJson(json, info, animInfo, sessionState, countdownMs, localInSession, localCharIndex);
	}

	json += "]}";

	if (m_callbacks) {
		m_callbacks->OnAnimationsAvailable(json.c_str());
	}
}
