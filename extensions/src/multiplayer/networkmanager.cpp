#include "extensions/multiplayer/networkmanager.h"

#include "legomain.h"
#include "legopathactor.h"
#include "legoworld.h"
#include "misc.h"
#include "mxmisc.h"
#include "mxticklemanager.h"
#include "roi/legoroi.h"

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <vector>

using namespace Multiplayer;

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
	: m_transport(nullptr), m_callbacks(nullptr), m_localPeerId(0), m_hostPeerId(0), m_sequence(0),
	  m_lastBroadcastTime(0), m_lastValidActorId(0), m_localWalkAnimId(0), m_localIdleAnimId(0),
	  m_localDisplayActorIndex(DISPLAY_ACTOR_NONE), m_inIsleWorld(false),
	  m_registered(false), m_pendingToggleThirdPerson(false), m_pendingWalkAnim(-1), m_pendingIdleAnim(-1),
	  m_pendingEmote(-1)
{
}

NetworkManager::~NetworkManager()
{
	Shutdown();
}

MxResult NetworkManager::Tickle()
{
	ProcessPendingRequests();
	m_thirdPersonCamera.Tick(0.016f);

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

bool NetworkManager::WasRejected() const
{
	return m_transport && m_transport->WasRejected();
}

void NetworkManager::OnWorldEnabled(LegoWorld* p_world)
{
	if (!p_world) {
		return;
	}

	if (!m_registered) {
		TickleManager()->RegisterClient(this, 10);
		m_registered = true;
	}

	m_thirdPersonCamera.OnWorldEnabled(p_world);

	if (p_world->GetWorldId() == LegoOmni::e_act1) {
		m_inIsleWorld = true;
		m_worldSync.SetInIsleWorld(true);

		for (auto& [peerId, player] : m_remotePlayers) {
			if (player->IsSpawned()) {
				player->ReAddToScene();

				if (player->GetWorldId() == (int8_t) LegoOmni::e_act1) {
					player->SetVisible(true);
				}
			}
		}

		NotifyPlayerCountChanged();
	}
}

void NetworkManager::OnWorldDisabled(LegoWorld* p_world)
{
	if (!p_world) {
		return;
	}

	m_thirdPersonCamera.OnWorldDisabled(p_world);

	if (p_world->GetWorldId() == LegoOmni::e_act1) {
		m_inIsleWorld = false;
		m_worldSync.SetInIsleWorld(false);
		for (auto& [peerId, player] : m_remotePlayers) {
			player->SetVisible(false);
		}

		NotifyPlayerCountChanged();
	}
}

MxBool NetworkManager::HandleEntityMutation(LegoEntity* p_entity, MxU8 p_changeType)
{
	return m_worldSync.HandleEntityMutation(p_entity, p_changeType);
}

void NetworkManager::ProcessPendingRequests()
{
	if (m_pendingToggleThirdPerson.exchange(false, std::memory_order_relaxed)) {
		if (m_thirdPersonCamera.IsEnabled()) {
			m_thirdPersonCamera.Disable();
		}
		else {
			m_thirdPersonCamera.Enable();
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

	PlayerStateMsg msg{};
	msg.header = {MSG_STATE, m_localPeerId, m_sequence++};
	msg.actorId = actorId;
	msg.worldId = (int8_t) currentWorld->GetWorldId();
	msg.vehicleType = DetectVehicleType(userActor);
	SDL_memcpy(msg.position, pos, sizeof(msg.position));
	SDL_memcpy(msg.direction, dir, sizeof(msg.direction));

	// When 3rd-person camera is active, ShouldInvertMovement causes movement
	// inversion, and CalculateTransform re-inverts to keep ROI z backward.
	// Negate to send the visual-forward direction that remote players expect.
	// RemotePlayer::UpdateTransform negates again to restore backward-z.
	if (m_thirdPersonCamera.IsActive()) {
		msg.direction[0] = -msg.direction[0];
		msg.direction[1] = -msg.direction[1];
		msg.direction[2] = -msg.direction[2];
	}

	SDL_memcpy(msg.up, up, sizeof(msg.up));
	msg.speed = speed;
	msg.walkAnimId = m_localWalkAnimId;
	msg.idleAnimId = m_localIdleAnimId;

	uint8_t displayIndex = m_localDisplayActorIndex;
	if (displayIndex == DISPLAY_ACTOR_NONE) {
		displayIndex = actorId - 1; // actorId already validated above
	}
	msg.displayActorIndex = displayIndex;

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
			break;
		}
		case MSG_HOST_ASSIGN: {
			HostAssignMsg msg;
			if (DeserializeMsg(data, length, msg) && msg.header.type == MSG_HOST_ASSIGN) {
				HandleHostAssign(msg);
			}
			break;
		}
		case MSG_JOIN: {
			PlayerJoinMsg msg;
			if (DeserializeMsg(data, length, msg) && msg.header.type == MSG_JOIN) {
				msg.name[sizeof(msg.name) - 1] = '\0';
				if (IsValidActorId(msg.actorId)) {
					HandleJoin(msg);
				}
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
	return ptr;
}

void NetworkManager::HandleJoin(const PlayerJoinMsg& p_msg)
{
	uint32_t peerId = p_msg.header.peerId;

	if (m_remotePlayers.count(peerId)) {
		return;
	}

	CreateAndSpawnPlayer(peerId, p_msg.actorId, p_msg.actorId - 1);
	NotifyPlayerCountChanged();
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

void NetworkManager::SetWalkAnimation(uint8_t p_index)
{
	if (p_index < g_walkAnimCount) {
		m_localWalkAnimId = p_index;
		m_thirdPersonCamera.SetWalkAnimId(p_index);
	}
}

void NetworkManager::SetIdleAnimation(uint8_t p_index)
{
	if (p_index < g_idleAnimCount) {
		m_localIdleAnimId = p_index;
		m_thirdPersonCamera.SetIdleAnimId(p_index);
	}
}

void NetworkManager::SendEmote(uint8_t p_emoteId)
{
	if (p_emoteId >= g_emoteAnimCount) {
		return;
	}

	m_thirdPersonCamera.TriggerEmote(p_emoteId);

	EmoteMsg msg{};
	msg.header = {MSG_EMOTE, m_localPeerId, m_sequence++};
	msg.emoteId = p_emoteId;
	SendMessage(msg);
}

void NetworkManager::SetDisplayActorIndex(uint8_t p_index)
{
	m_localDisplayActorIndex = p_index;
	m_thirdPersonCamera.SetDisplayActorIndex(p_index);
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
	NotifyPlayerCountChanged();
}

void NetworkManager::NotifyPlayerCountChanged()
{
	if (!m_callbacks) {
		return;
	}

	int count = -1;
	if (m_inIsleWorld) {
		count = 1; // local player
		for (auto& [peerId, player] : m_remotePlayers) {
			if (player->GetWorldId() == (int8_t) LegoOmni::e_act1) {
				count++;
			}
		}
	}

	m_callbacks->OnPlayerCountChanged(count);
}

