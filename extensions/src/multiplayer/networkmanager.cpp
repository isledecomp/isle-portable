#include "extensions/multiplayer/networkmanager.h"

#include "legobuildingmanager.h"
#include "legoentity.h"
#include "legomain.h"
#include "legopathactor.h"
#include "legoplantmanager.h"
#include "legoplants.h"
#include "legoworld.h"
#include "misc.h"
#include "misc/legostorage.h"
#include "mxmisc.h"
#include "mxticklemanager.h"
#include "roi/legoroi.h"

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <vector>

extern MxU8 g_counters[];
extern MxU8 g_buildingInfoDownshift[];

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
	  m_lastValidActorId(0), m_inIsleWorld(false), m_registered(false), m_snapshotRequested(false)
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

	if (!m_registered) {
		TickleManager()->RegisterClient(this, 10);
		m_registered = true;
	}

	if (p_world->GetWorldId() == LegoOmni::e_act1) {
		m_inIsleWorld = true;

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
				HandleRequestSnapshot(msg);
			}
			break;
		}
		case MSG_WORLD_SNAPSHOT: {
			HandleWorldSnapshot(data, length);
			break;
		}
		case MSG_WORLD_EVENT: {
			WorldEventMsg msg;
			if (DeserializeMsg(data, length, msg) && msg.header.type == MSG_WORLD_EVENT) {
				HandleWorldEvent(msg);
			}
			break;
		}
		case MSG_WORLD_EVENT_REQUEST: {
			WorldEventRequestMsg msg;
			if (DeserializeMsg(data, length, msg) && msg.header.type == MSG_WORLD_EVENT_REQUEST) {
				HandleWorldEventRequest(msg);
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

	// If the host changed and we're not the new host, request a snapshot.
	// Reset any pending snapshot state since the old host may have disconnected
	// before responding to our previous request.
	if (!IsHost() && oldHost != m_hostPeerId) {
		m_snapshotRequested = false;
		m_pendingWorldEvents.clear();
		SendSnapshotRequest();
	}
}

void NetworkManager::HandleRequestSnapshot(const RequestSnapshotMsg& p_msg)
{
	// Only the host should respond to snapshot requests
	if (!IsHost()) {
		return;
	}

	SendWorldSnapshot(p_msg.header.peerId);
}

void NetworkManager::HandleWorldSnapshot(const uint8_t* p_data, size_t p_length)
{
	WorldSnapshotMsg header;
	if (!DeserializeMsg(p_data, p_length, header) || header.header.type != MSG_WORLD_SNAPSHOT) {
		return;
	}

	if (p_length < sizeof(WorldSnapshotMsg) + header.dataLength) {
		return;
	}

	const uint8_t* snapshotData = p_data + sizeof(WorldSnapshotMsg);

	// Apply the snapshot using LegoMemory with the existing Read() methods
	LegoMemory memory((void*) snapshotData, header.dataLength);

	PlantManager()->Read(&memory);
	BuildingManager()->Read(&memory);

	// If we're in the Isle world, update entity visuals after applying the snapshot.
	// Read() calls AdjustHeight() which updates data arrays, but doesn't update
	// entity positions. We need to reload world info to refresh visuals.
	if (m_inIsleWorld) {
		// Reset and reload plant entities with the new data
		LegoWorld* world = CurrentWorld();
		if (world && world->GetWorldId() == LegoOmni::e_act1) {
			PlantManager()->Reset(LegoOmni::e_act1);
			PlantManager()->LoadWorldInfo(LegoOmni::e_act1);
			BuildingManager()->Reset();
			BuildingManager()->LoadWorldInfo();
		}
	}

	// Apply any world events that were queued between snapshot request and response
	for (const auto& evt : m_pendingWorldEvents) {
		ApplyWorldEvent(evt.entityType, evt.changeType, evt.entityIndex);
	}
	m_pendingWorldEvents.clear();
	m_snapshotRequested = false;
}

void NetworkManager::HandleWorldEvent(const WorldEventMsg& p_msg)
{
	// If we're waiting for a snapshot, queue this event for later
	if (m_snapshotRequested) {
		m_pendingWorldEvents.push_back(p_msg);
		return;
	}

	ApplyWorldEvent(p_msg.entityType, p_msg.changeType, p_msg.entityIndex);
}

void NetworkManager::HandleWorldEventRequest(const WorldEventRequestMsg& p_msg)
{
	// Only the host processes event requests
	if (!IsHost()) {
		return;
	}

	// Apply locally on the host
	ApplyWorldEvent(p_msg.entityType, p_msg.changeType, p_msg.entityIndex);

	// Broadcast to all peers as an authoritative world event
	BroadcastWorldEvent(p_msg.entityType, p_msg.changeType, p_msg.entityIndex);
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

bool NetworkManager::IsInIsleWorld() const
{
	return m_inIsleWorld;
}

// ---- World state sync ----

template <typename TInfo>
static int FindEntityIndex(TInfo* p_infoArray, MxS32 p_count, LegoEntity* p_entity)
{
	for (MxS32 i = 0; i < p_count; i++) {
		if (p_infoArray[i].m_entity == p_entity) {
			return i;
		}
	}
	return -1;
}

MxBool NetworkManager::HandleEntityMutation(LegoEntity* p_entity, MxU8 p_changeType)
{
	if (!IsConnected()) {
		return FALSE;
	}

	uint8_t entityType;
	int idx;

	if (p_entity->GetType() == LegoEntity::e_plant) {
		entityType = ENTITY_PLANT;
		MxS32 count;
		idx = FindEntityIndex(PlantManager()->GetInfoArray(count), count, p_entity);
	}
	else if (p_entity->GetType() == LegoEntity::e_building) {
		entityType = ENTITY_BUILDING;
		MxS32 count;
		idx = FindEntityIndex(BuildingManager()->GetInfoArray(count), count, p_entity);
	}
	else {
		return FALSE;
	}

	if (idx < 0) {
		return FALSE;
	}

	if (IsHost()) {
		// Host: allow local mutation, then broadcast to all peers
		BroadcastWorldEvent(entityType, p_changeType, (uint8_t) idx);
		return FALSE; // FALSE = allow local mutation to proceed
	}
	else {
		// Non-host: send request to host, block local mutation
		SendWorldEventRequest(entityType, p_changeType, (uint8_t) idx);
		return TRUE; // TRUE = suppress local mutation
	}
}

void NetworkManager::SendSnapshotRequest()
{
	RequestSnapshotMsg msg{};
	msg.header = {MSG_REQUEST_SNAPSHOT, m_localPeerId, m_sequence++};
	SendMessage(msg);

	m_snapshotRequested = true;
	m_pendingWorldEvents.clear();
}

void NetworkManager::SendWorldSnapshot(uint32_t p_targetPeerId)
{
	if (!m_transport || !m_transport->IsConnected()) {
		return;
	}

	// Serialize plant + building state into a buffer using existing Write() methods
	// Max sizes: 81 plants * (1+4+4+1+1+1) = 81*12 = 972 bytes
	//            16 buildings * (4+4+1+1) = 16*10 = 160 bytes + 1 byte nextVariant
	// Total ~1133 bytes. Use 4096 for safety.
	uint8_t stateBuffer[4096];
	LegoMemory memory(stateBuffer, sizeof(stateBuffer));

	PlantManager()->Write(&memory);
	BuildingManager()->Write(&memory);

	LegoU32 dataLength;
	memory.GetPosition(dataLength);

	// Build the snapshot header + trailing payload
	WorldSnapshotMsg msg{};
	msg.header = {MSG_WORLD_SNAPSHOT, m_localPeerId, m_sequence++};
	msg.targetPeerId = p_targetPeerId;
	msg.dataLength = (uint16_t) dataLength;

	std::vector<uint8_t> msgBuf(sizeof(WorldSnapshotMsg) + dataLength);
	SDL_memcpy(msgBuf.data(), &msg, sizeof(WorldSnapshotMsg));
	SDL_memcpy(msgBuf.data() + sizeof(WorldSnapshotMsg), stateBuffer, dataLength);

	m_transport->Send(msgBuf.data(), msgBuf.size());
}

void NetworkManager::BroadcastWorldEvent(uint8_t p_entityType, uint8_t p_changeType, uint8_t p_entityIndex)
{
	WorldEventMsg msg{};
	msg.header = {MSG_WORLD_EVENT, m_localPeerId, m_sequence++};
	msg.entityType = p_entityType;
	msg.changeType = p_changeType;
	msg.entityIndex = p_entityIndex;
	SendMessage(msg);
}

void NetworkManager::SendWorldEventRequest(uint8_t p_entityType, uint8_t p_changeType, uint8_t p_entityIndex)
{
	WorldEventRequestMsg msg{};
	msg.header = {MSG_WORLD_EVENT_REQUEST, m_localPeerId, m_sequence++};
	msg.entityType = p_entityType;
	msg.changeType = p_changeType;
	msg.entityIndex = p_entityIndex;
	SendMessage(msg);
}

// Dispatch Switch*() calls shared by all entity types.
// Returns true if the change was handled, false for type-specific changes.
static bool DispatchEntitySwitch(LegoEntity* p_entity, uint8_t p_changeType)
{
	switch (p_changeType) {
	case CHANGE_VARIANT:
		p_entity->SwitchVariant();
		return true;
	case CHANGE_SOUND:
		p_entity->SwitchSound();
		return true;
	case CHANGE_MOVE:
		p_entity->SwitchMove();
		return true;
	case CHANGE_MOOD:
		p_entity->SwitchMood();
		return true;
	default:
		return false;
	}
}

void NetworkManager::ApplyWorldEvent(uint8_t p_entityType, uint8_t p_changeType, uint8_t p_entityIndex)
{
	if (p_entityType == ENTITY_PLANT) {
		MxS32 numPlants;
		LegoPlantInfo* plantInfo = PlantManager()->GetInfoArray(numPlants);
		if (p_entityIndex >= numPlants) {
			return;
		}

		LegoPlantInfo* info = &plantInfo[p_entityIndex];

		// If entity exists (we're in the Isle world), use LegoEntity::Switch*()
		// which handles data mutation + visual update + sound + animation + counter
		if (info->m_entity != NULL) {
			if (!DispatchEntitySwitch(info->m_entity, p_changeType)) {
				if (p_changeType == CHANGE_COLOR) {
					info->m_entity->SwitchColor(info->m_entity->GetROI());
				}
				else if (p_changeType == CHANGE_DECREMENT) {
					PlantManager()->DecrementCounter(info->m_entity);
				}
			}
		}
		else {
			// Entity is NULL (we're outside the Isle world).
			// Apply changes directly to the data array.
			switch (p_changeType) {
			case CHANGE_VARIANT:
				if (info->m_counter == -1) {
					info->m_variant++;
					if (info->m_variant > LegoPlantInfo::e_palm) {
						info->m_variant = LegoPlantInfo::e_flower;
					}

					// Clamp move to the new variant's max (mirrors SwitchVariant)
					if (info->m_move != 0 && info->m_move >= (MxU32) LegoPlantManager::g_maxMove[info->m_variant]) {
						info->m_move = LegoPlantManager::g_maxMove[info->m_variant] - 1;
					}
				}
				break;
			case CHANGE_SOUND:
				info->m_sound++;
				if (info->m_sound >= LegoPlantManager::g_maxSound) {
					info->m_sound = 0;
				}
				break;
			case CHANGE_MOVE:
				info->m_move++;
				if (info->m_move >= (MxU32) LegoPlantManager::g_maxMove[info->m_variant]) {
					info->m_move = 0;
				}
				break;
			case CHANGE_COLOR:
				info->m_color++;
				if (info->m_color > LegoPlantInfo::e_green) {
					info->m_color = LegoPlantInfo::e_white;
				}
				break;
			case CHANGE_MOOD:
				info->m_mood++;
				if (info->m_mood > 3) {
					info->m_mood = 0;
				}
				break;
			case CHANGE_DECREMENT: {
				if (info->m_counter < 0) {
					info->m_counter = g_counters[info->m_variant];
				}
				if (info->m_counter > 0) {
					info->m_counter--;
					if (info->m_counter == 1) {
						info->m_counter = 0;
					}
				}
				break;
			}
			}
		}
	}
	else if (p_entityType == ENTITY_BUILDING) {
		MxS32 numBuildings;
		LegoBuildingInfo* buildingInfo = BuildingManager()->GetInfoArray(numBuildings);
		if (p_entityIndex >= numBuildings) {
			return;
		}

		LegoBuildingInfo* info = &buildingInfo[p_entityIndex];

		// If entity exists (we're in the Isle world), use LegoEntity::Switch*()
		if (info->m_entity != NULL) {
			if (!DispatchEntitySwitch(info->m_entity, p_changeType)) {
				if (p_changeType == CHANGE_COLOR) {
					info->m_entity->SwitchColor(info->m_entity->GetROI());
				}
				else if (p_changeType == CHANGE_DECREMENT) {
					BuildingManager()->DecrementCounter(info->m_entity);
				}
			}
		}
		else {
			// Entity is NULL (we're outside the Isle world).
			// Apply changes directly to the data array.
			switch (p_changeType) {
			case CHANGE_SOUND:
				if (info->m_flags & LegoBuildingInfo::c_hasSounds) {
					info->m_sound++;
					if (info->m_sound >= LegoBuildingManager::g_maxSound) {
						info->m_sound = 0;
					}
				}
				break;
			case CHANGE_MOVE:
				if (info->m_flags & LegoBuildingInfo::c_hasMoves) {
					info->m_move++;
					if (info->m_move >= (MxU32) LegoBuildingManager::g_maxMove[p_entityIndex]) {
						info->m_move = 0;
					}
				}
				break;
			case CHANGE_MOOD:
				if (info->m_flags & LegoBuildingInfo::c_hasMoods) {
					info->m_mood++;
					if (info->m_mood > 3) {
						info->m_mood = 0;
					}
				}
				break;
			case CHANGE_DECREMENT: {
				if (info->m_counter < 0) {
					info->m_counter = g_buildingInfoDownshift[p_entityIndex];
				}
				if (info->m_counter > 0) {
					info->m_counter -= 2;
					if (info->m_counter == 1) {
						info->m_counter = 0;
					}
				}
				break;
			}
			case CHANGE_VARIANT:
			case CHANGE_COLOR:
				// Variant switching is config-dependent, color N/A for buildings
				break;
			}
		}
	}
}
