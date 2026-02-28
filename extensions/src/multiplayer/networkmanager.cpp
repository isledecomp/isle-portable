#include "extensions/multiplayer/networkmanager.h"

#include "legomain.h"
#include "legopathactor.h"
#include "legoworld.h"
#include "misc.h"
#include "mxmisc.h"
#include "mxticklemanager.h"
#include "roi/legoroi.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <vector>

using namespace Multiplayer;

NetworkManager::NetworkManager()
	: m_transport(nullptr), m_localPeerId(0), m_sequence(0), m_lastBroadcastTime(0), m_lastValidActorId(0),
	  m_inIsleWorld(false), m_registered(false)
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

	// Timeout check - remove stale remote players.
	// Re-read time because ProcessIncomingPackets updates player timestamps
	// via SDL_GetTicks(), which may be newer than the 'now' captured above.
	// Using the stale 'now' would cause unsigned underflow (now < lastUpdate).
	uint32_t timeoutNow = SDL_GetTicks();
	std::vector<uint32_t> timedOut;
	for (auto& [peerId, player] : m_remotePlayers) {
		uint32_t lastUpdate = player->GetLastUpdateTime();
		if (timeoutNow >= lastUpdate && (timeoutNow - lastUpdate) > TIMEOUT_MS) {
			SDL_Log("Multiplayer: peer %u timed out", peerId);
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

	SDL_Log("Multiplayer: OnWorldEnabled worldId=%d (e_act1=%d)", p_world->GetWorldId(), LegoOmni::e_act1);

	// Register with tickle manager on first world enable (engine is now initialized)
	if (!m_registered) {
		TickleManager()->RegisterClient(this, 10);
		m_registered = true;
	}

	if (p_world->GetWorldId() == LegoOmni::e_act1) {
		m_inIsleWorld = true;

		// Re-add all remote player ROIs to the 3D scene
		for (auto& [peerId, player] : m_remotePlayers) {
			if (player->IsSpawned()) {
				player->ReAddToScene();

				// Only show if the remote player is also in ISLE
				if (player->GetWorldId() == (int8_t) LegoOmni::e_act1) {
					player->SetVisible(true);
				}
			}
		}

		SDL_Log("Multiplayer: ISLE world enabled, re-added %zu remote players", m_remotePlayers.size());
	}
}

void NetworkManager::OnWorldDisabled(LegoWorld* p_world)
{
	if (!p_world) {
		return;
	}

	if (p_world->GetWorldId() == LegoOmni::e_act1) {
		m_inIsleWorld = false;
		for (auto& [peerId, player] : m_remotePlayers) {
			player->SetVisible(false);
		}
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

	// Don't broadcast if we haven't seen a valid character yet
	if (!IsValidActorId(actorId)) {
		return;
	}

	int8_t worldId = (int8_t) currentWorld->GetWorldId();
	int8_t vehicleType = DetectLocalVehicleType();

	// Log first broadcast per session for debugging
	static bool firstBroadcast = true;
	if (firstBroadcast) {
		SDL_Log(
			"Multiplayer: first broadcast actorId=%u worldId=%d vehicleType=%d pos=(%.1f,%.1f,%.1f)",
			actorId,
			worldId,
			vehicleType,
			pos[0],
			pos[1],
			pos[2]
		);
		firstBroadcast = false;
	}

	uint8_t buf[64];
	size_t len = SerializeStateMsg(
		buf,
		sizeof(buf),
		m_localPeerId,
		m_sequence++,
		actorId,
		worldId,
		vehicleType,
		pos,
		dir,
		up,
		speed
	);

	if (len > 0) {
		m_transport->Send(buf, len);
	}
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
				SDL_Log("Multiplayer: assigned peer ID %u", m_localPeerId);
			}
			break;
		}
		case MSG_JOIN: {
			PlayerJoinMsg msg;
			if (DeserializeJoinMsg(data, length, msg)) {
				HandleJoin(msg);
			}
			break;
		}
		case MSG_LEAVE: {
			PlayerLeaveMsg msg;
			if (DeserializeLeaveMsg(data, length, msg)) {
				HandleLeave(msg);
			}
			break;
		}
		case MSG_STATE: {
			PlayerStateMsg msg;
			if (DeserializeStateMsg(data, length, msg)) {
				HandleState(msg);
			}
			break;
		}
		default:
			SDL_Log("Multiplayer: unknown message type %u (len=%zu)", msgType, length);
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

void NetworkManager::HandleJoin(const PlayerJoinMsg& p_msg)
{
	uint32_t peerId = p_msg.header.peerId;

	if (m_remotePlayers.count(peerId)) {
		return; // Already known
	}

	SDL_Log("Multiplayer: peer %u joined as actor %d (%s)", peerId, p_msg.actorId, p_msg.name);

	auto player = std::make_unique<RemotePlayer>(peerId, p_msg.actorId);

	// Spawn in current world if we're in ISLE
	if (m_inIsleWorld) {
		LegoWorld* world = CurrentWorld();
		if (world && world->GetWorldId() == LegoOmni::e_act1) {
			player->Spawn(world);
		}
	}

	m_remotePlayers[peerId] = std::move(player);
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
		// Auto-create remote player on first STATE if we haven't seen a JOIN
		if (!IsValidActorId(p_msg.actorId)) {
			return;
		}

		SDL_Log("Multiplayer: new remote peer %u (actor %u)", peerId, p_msg.actorId);
		auto player = std::make_unique<RemotePlayer>(peerId, p_msg.actorId);

		if (m_inIsleWorld) {
			LegoWorld* world = CurrentWorld();
			if (world && world->GetWorldId() == LegoOmni::e_act1) {
				player->Spawn(world);
			}
		}

		m_remotePlayers[peerId] = std::move(player);
		it = m_remotePlayers.find(peerId);
	}

	// Handle actor change (e.g., Pepper -> Nick): despawn and respawn with new actor
	if (IsValidActorId(p_msg.actorId) && it->second->GetActorId() != p_msg.actorId) {
		SDL_Log("Multiplayer: peer %u changed actor %u -> %u", peerId, it->second->GetActorId(), p_msg.actorId);
		it->second->Despawn();
		auto player = std::make_unique<RemotePlayer>(peerId, p_msg.actorId);

		if (m_inIsleWorld) {
			LegoWorld* world = CurrentWorld();
			if (world && world->GetWorldId() == LegoOmni::e_act1) {
				player->Spawn(world);
			}
		}

		m_remotePlayers[peerId] = std::move(player);
		it = m_remotePlayers.find(peerId);
	}

	it->second->UpdateFromNetwork(p_msg);

	// Handle visibility based on worldId
	bool bothInIsle = m_inIsleWorld && (p_msg.worldId == (int8_t) LegoOmni::e_act1);

	if (it->second->IsSpawned()) {
		bool wasVisible = it->second->IsVisible();
		it->second->SetVisible(bothInIsle);
		if (wasVisible != bothInIsle) {
			SDL_Log(
				"Multiplayer: peer %u visibility %d->%d (inIsle=%d, msgWorld=%d, e_act1=%d, spawned=%d)",
				peerId,
				wasVisible,
				bothInIsle,
				m_inIsleWorld,
				p_msg.worldId,
				(int8_t) LegoOmni::e_act1,
				it->second->IsSpawned()
			);
		}
	}
	else {
		SDL_Log("Multiplayer: peer %u not spawned, skipping visibility (inIsle=%d)", peerId, m_inIsleWorld);
	}
}

void NetworkManager::RemoveRemotePlayer(uint32_t p_peerId)
{
	auto it = m_remotePlayers.find(p_peerId);
	if (it != m_remotePlayers.end()) {
		SDL_Log("Multiplayer: peer %u removed", p_peerId);
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
	LegoPathActor* actor = UserActor();
	if (!actor) {
		return VEHICLE_NONE;
	}

	if (actor->IsA("Helicopter")) {
		return VEHICLE_HELICOPTER;
	}
	if (actor->IsA("Jetski")) {
		return VEHICLE_JETSKI;
	}
	if (actor->IsA("DuneBuggy")) {
		return VEHICLE_DUNEBUGGY;
	}
	if (actor->IsA("Bike")) {
		return VEHICLE_BIKE;
	}
	if (actor->IsA("SkateBoard")) {
		return VEHICLE_SKATEBOARD;
	}
	if (actor->IsA("Motocycle")) {
		return VEHICLE_MOTOCYCLE;
	}
	if (actor->IsA("TowTrack")) {
		return VEHICLE_TOWTRACK;
	}
	if (actor->IsA("Ambulance")) {
		return VEHICLE_AMBULANCE;
	}

	return VEHICLE_NONE;
}

bool NetworkManager::IsInIsleWorld() const
{
	return m_inIsleWorld;
}
