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
	: m_transport(nullptr), m_localPeerId(0), m_hostPeerId(0), m_sequence(0), m_lastBroadcastTime(0),
	  m_lastValidActorId(0), m_inIsleWorld(false), m_registered(false)
{
}

NetworkManager::~NetworkManager()
{
	Shutdown();
}

MxResult NetworkManager::Tickle()
{
	if (!m_transport) {
		return SUCCESS;
	}

	uint32_t now = SDL_GetTicks();

	// Broadcast BEFORE receiving: the Send proxy call gives the main thread a
	// chance to process incoming WebSocket onmessage events before we drain
	// the queue with Receive.
	if (m_transport->IsConnected() && (now - m_lastBroadcastTime) >= BROADCAST_INTERVAL_MS) {
		BroadcastLocalState();
		m_lastBroadcastTime = now;
	}

	ProcessIncomingPackets();
	UpdateRemotePlayers(0.016f);

	// Re-read time because ProcessIncomingPackets updates player timestamps
	// via SDL_GetTicks(), which may be newer than the 'now' captured above.
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

void NetworkManager::Initialize(NetworkTransport* p_transport)
{
	m_transport = p_transport;
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

void NetworkManager::OnWorldEnabled(LegoWorld* p_world)
{
	if (!p_world) {
		return;
	}

	if (!m_registered) {
		TickleManager()->RegisterClient(this, 10);
		m_registered = true;
	}

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
		for (auto& [peerId, player] : m_remotePlayers) {
			player->SetVisible(false);
		}
	}
}

MxBool NetworkManager::HandleEntityMutation(LegoEntity* p_entity, MxU8 p_changeType)
{
	return m_worldSync.HandleEntityMutation(p_entity, p_changeType);
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
	msg.vehicleType = DetectLocalVehicleType();
	SDL_memcpy(msg.position, pos, sizeof(msg.position));
	SDL_memcpy(msg.direction, dir, sizeof(msg.direction));
	SDL_memcpy(msg.up, up, sizeof(msg.up));
	msg.speed = speed;

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

RemotePlayer* NetworkManager::CreateAndSpawnPlayer(uint32_t p_peerId, uint8_t p_actorId)
{
	auto player = std::make_unique<RemotePlayer>(p_peerId, p_actorId);

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

	CreateAndSpawnPlayer(peerId, p_msg.actorId);
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

		CreateAndSpawnPlayer(peerId, p_msg.actorId);
		it = m_remotePlayers.find(peerId);
	}

	// Handle actor change (e.g., Pepper -> Nick)
	if (IsValidActorId(p_msg.actorId) && it->second->GetActorId() != p_msg.actorId) {
		it->second->Despawn();
		m_remotePlayers.erase(it);
		CreateAndSpawnPlayer(peerId, p_msg.actorId);
		it = m_remotePlayers.find(peerId);
	}

	it->second->UpdateFromNetwork(p_msg);

	bool bothInIsle = m_inIsleWorld && (p_msg.worldId == (int8_t) LegoOmni::e_act1);
	if (it->second->IsSpawned()) {
		it->second->SetVisible(bothInIsle);
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

void NetworkManager::RemoveRemotePlayer(uint32_t p_peerId)
{
	auto it = m_remotePlayers.find(p_peerId);
	if (it != m_remotePlayers.end()) {
		it->second->Despawn();
		m_remotePlayers.erase(it);
	}
}

void NetworkManager::RemoveAllRemotePlayers()
{
	for (auto& [peerId, player] : m_remotePlayers) {
		player->Despawn();
	}
	m_remotePlayers.clear();
}

int8_t NetworkManager::DetectLocalVehicleType()
{
	static const struct {
		const char* className;
		int8_t vehicleType;
	} vehicleMap[] = {
		{"Helicopter", VEHICLE_HELICOPTER},
		{"Jetski", VEHICLE_JETSKI},
		{"DuneBuggy", VEHICLE_DUNEBUGGY},
		{"Bike", VEHICLE_BIKE},
		{"SkateBoard", VEHICLE_SKATEBOARD},
		{"Motorcycle", VEHICLE_MOTOCYCLE},
		{"TowTrack", VEHICLE_TOWTRACK},
		{"Ambulance", VEHICLE_AMBULANCE},
	};

	LegoPathActor* actor = UserActor();
	if (!actor) {
		return VEHICLE_NONE;
	}

	for (const auto& entry : vehicleMap) {
		if (actor->IsA(entry.className)) {
			return entry.vehicleType;
		}
	}
	return VEHICLE_NONE;
}
