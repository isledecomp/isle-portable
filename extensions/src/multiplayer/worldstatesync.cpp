#include "extensions/multiplayer/worldstatesync.h"

#include "isle.h"
#include "legobuildingmanager.h"
#include "legoentity.h"
#include "legogamestate.h"
#include "legomain.h"
#include "legoplantmanager.h"
#include "legoplants.h"
#include "legoutils.h"
#include "legoworld.h"
#include "misc.h"
#include "misc/legostorage.h"
#include "mxmisc.h"
#include "mxvariable.h"
#include "mxvariabletable.h"

#include <SDL3/SDL_stdinc.h>
#include <cstdio>
#include <cstdlib>
#include <vector>

extern MxU8 g_counters[];
extern MxU8 g_buildingInfoDownshift[];

using namespace Multiplayer;

template <typename T>
void WorldStateSync::SendMessage(const T& p_msg)
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

WorldStateSync::WorldStateSync()
	: m_transport(nullptr), m_localPeerId(0), m_sequence(0), m_isHost(false), m_inIsleWorld(false),
	  m_snapshotRequested(false), m_savedLightPos(2)
{
}

void WorldStateSync::SaveSkyLightState()
{
	const char* bgValue = GameState()->GetBackgroundColor()->GetValue()->GetData();
	m_savedSkyColor = bgValue ? bgValue : "set 56 54 68";
	m_savedLightPos = atoi(VariableTable()->GetVariable("lightposition"));
}

void WorldStateSync::RestoreSkyLightState()
{
	ApplySkyLightState(m_savedSkyColor.c_str(), m_savedLightPos);
}

void WorldStateSync::ApplySkyLightState(const char* p_skyColor, int p_lightPos)
{
	GameState()->GetBackgroundColor()->SetValue(p_skyColor);
	GameState()->GetBackgroundColor()->SetLightColor();
	SetLightPosition(p_lightPos);

	char buf[32];
	sprintf(buf, "%d", p_lightPos);
	VariableTable()->SetVariable("lightposition", buf);
}

void WorldStateSync::OnHostChanged()
{
	if (!m_isHost) {
		m_snapshotRequested = false;
		m_pendingWorldEvents.clear();
		SendSnapshotRequest();
	}
}

void WorldStateSync::SendWorldSnapshotTo(uint32_t p_targetPeerId)
{
	SendWorldSnapshot(p_targetPeerId);
}

void WorldStateSync::HandleRequestSnapshot(const RequestSnapshotMsg& p_msg)
{
	if (!m_isHost) {
		return;
	}

	SendWorldSnapshot(p_msg.header.peerId);
}

void WorldStateSync::HandleWorldSnapshot(const uint8_t* p_data, size_t p_length)
{
	WorldSnapshotMsg header;
	if (!DeserializeMsg(p_data, p_length, header) || header.header.type != MSG_WORLD_SNAPSHOT) {
		return;
	}

	if (p_length < sizeof(WorldSnapshotMsg) + header.dataLength) {
		return;
	}

	const uint8_t* snapshotData = p_data + sizeof(WorldSnapshotMsg);

	// Apply the snapshot via LegoMemory.
	LegoMemory memory((void*) snapshotData, header.dataLength);

	PlantManager()->Read(&memory);
	BuildingManager()->Read(&memory);

	// Read sky/light state appended after plant + building data.
	LegoU32 memPos;
	memory.GetPosition(memPos);
	const uint8_t* extraData = snapshotData + memPos;
	size_t remaining = header.dataLength - memPos;

	if (remaining >= 4) {
		char skyBuffer[32];
		sprintf(skyBuffer, "set %d %d %d", extraData[0], extraData[1], extraData[2]);
		ApplySkyLightState(skyBuffer, extraData[3]);
	}

	// Read() updates data arrays but not entity positions; reload to refresh.
	if (m_inIsleWorld) {
		LegoWorld* world = CurrentWorld();
		if (world && world->GetWorldId() == LegoOmni::e_act1) {
			PlantManager()->Reset(LegoOmni::e_act1);
			PlantManager()->LoadWorldInfo(LegoOmni::e_act1);
			BuildingManager()->Reset();
			BuildingManager()->LoadWorldInfo();
		}
	}

	// Replay events queued while snapshot was in flight.
	for (const auto& evt : m_pendingWorldEvents) {
		ApplyWorldEvent(evt.entityType, evt.changeType, evt.entityIndex);
	}
	m_pendingWorldEvents.clear();
	m_snapshotRequested = false;
}

void WorldStateSync::HandleWorldEvent(const WorldEventMsg& p_msg)
{
	if (m_snapshotRequested) {
		m_pendingWorldEvents.push_back(p_msg);
		return;
	}

	ApplyWorldEvent(p_msg.entityType, p_msg.changeType, p_msg.entityIndex);
}

void WorldStateSync::HandleWorldEventRequest(const WorldEventRequestMsg& p_msg)
{
	if (!m_isHost) {
		return;
	}

	ApplyWorldEvent(p_msg.entityType, p_msg.changeType, p_msg.entityIndex);
	BroadcastWorldEvent(p_msg.entityType, p_msg.changeType, p_msg.entityIndex);
}

// ---- Entity mutation routing ----

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

MxBool WorldStateSync::HandleEntityMutation(LegoEntity* p_entity, MxU8 p_changeType)
{
	if (!m_transport || !m_transport->IsConnected()) {
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

	if (m_isHost) {
		BroadcastWorldEvent(entityType, p_changeType, (uint8_t) idx);
		return FALSE;
	}
	else {
		SendWorldEventRequest(entityType, p_changeType, (uint8_t) idx);
		return TRUE;
	}
}

MxBool WorldStateSync::HandleSkyLightMutation(uint8_t p_entityType, uint8_t p_changeType)
{
	if (!m_transport || !m_transport->IsConnected()) {
		return FALSE;
	}

	if (m_isHost) {
		BroadcastWorldEvent(p_entityType, p_changeType, 0);
		return FALSE;
	}
	else {
		SendWorldEventRequest(p_entityType, p_changeType, 0);
		return TRUE;
	}
}

// ---- Send helpers ----

void WorldStateSync::SendSnapshotRequest()
{
	RequestSnapshotMsg msg{};
	msg.header = {MSG_REQUEST_SNAPSHOT, m_localPeerId, m_sequence++};
	SendMessage(msg);

	m_snapshotRequested = true;
	m_pendingWorldEvents.clear();
}

void WorldStateSync::SendWorldSnapshot(uint32_t p_targetPeerId)
{
	if (!m_transport || !m_transport->IsConnected()) {
		return;
	}

	// Serialize plant + building + sky/light state (~1149 bytes max, use 4096 for safety).
	uint8_t stateBuffer[4096];
	LegoMemory memory(stateBuffer, sizeof(stateBuffer));

	PlantManager()->Write(&memory);
	BuildingManager()->Write(&memory);

	LegoU32 dataLength;
	memory.GetPosition(dataLength);

	// Append sky color HSV (parse from "set H S V" string) and light position.
	int skyH = 56, skyS = 54, skyV = 68; // defaults matching "set 56 54 68"
	const char* bgValue = GameState()->GetBackgroundColor()->GetValue()->GetData();
	if (bgValue) {
		sscanf(bgValue, "set %d %d %d", &skyH, &skyS, &skyV);
	}

	int lightPos = atoi(VariableTable()->GetVariable("lightposition"));

	stateBuffer[dataLength++] = (uint8_t) skyH;
	stateBuffer[dataLength++] = (uint8_t) skyS;
	stateBuffer[dataLength++] = (uint8_t) skyV;
	stateBuffer[dataLength++] = (uint8_t) lightPos;

	WorldSnapshotMsg msg{};
	msg.header = {MSG_WORLD_SNAPSHOT, m_localPeerId, m_sequence++};
	msg.targetPeerId = p_targetPeerId;
	msg.dataLength = (uint16_t) dataLength;

	std::vector<uint8_t> msgBuf(sizeof(WorldSnapshotMsg) + dataLength);
	SDL_memcpy(msgBuf.data(), &msg, sizeof(WorldSnapshotMsg));
	SDL_memcpy(msgBuf.data() + sizeof(WorldSnapshotMsg), stateBuffer, dataLength);

	m_transport->Send(msgBuf.data(), msgBuf.size());
}

void WorldStateSync::BroadcastWorldEvent(uint8_t p_entityType, uint8_t p_changeType, uint8_t p_entityIndex)
{
	WorldEventMsg msg{};
	msg.header = {MSG_WORLD_EVENT, m_localPeerId, m_sequence++};
	msg.entityType = p_entityType;
	msg.changeType = p_changeType;
	msg.entityIndex = p_entityIndex;
	SendMessage(msg);
}

void WorldStateSync::SendWorldEventRequest(uint8_t p_entityType, uint8_t p_changeType, uint8_t p_entityIndex)
{
	WorldEventRequestMsg msg{};
	msg.header = {MSG_WORLD_EVENT_REQUEST, m_localPeerId, m_sequence++};
	msg.entityType = p_entityType;
	msg.changeType = p_changeType;
	msg.entityIndex = p_entityIndex;
	SendMessage(msg);
}

// ---- Apply world events ----

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

void WorldStateSync::ApplyWorldEvent(uint8_t p_entityType, uint8_t p_changeType, uint8_t p_entityIndex)
{
	if (p_entityType == ENTITY_PLANT) {
		MxS32 numPlants;
		LegoPlantInfo* plantInfo = PlantManager()->GetInfoArray(numPlants);
		if (p_entityIndex >= numPlants) {
			return;
		}

		LegoPlantInfo* info = &plantInfo[p_entityIndex];

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
			switch (p_changeType) {
			case CHANGE_VARIANT:
				if (info->m_counter == -1) {
					info->m_variant++;
					if (info->m_variant > LegoPlantInfo::e_palm) {
						info->m_variant = LegoPlantInfo::e_flower;
					}

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
	else if (p_entityType == ENTITY_SKY) {
		switch (p_changeType) {
		case SKY_TOGGLE_COLOR:
			GameState()->GetBackgroundColor()->ToggleSkyColor();
			break;
		case SKY_DAY:
			GameState()->GetBackgroundColor()->ToggleDayNight(TRUE);
			break;
		case SKY_NIGHT:
			GameState()->GetBackgroundColor()->ToggleDayNight(FALSE);
			break;
		}
	}
	else if (p_entityType == ENTITY_LIGHT) {
		switch (p_changeType) {
		case LIGHT_INCREMENT:
			UpdateLightPosition(1);
			break;
		case LIGHT_DECREMENT:
			UpdateLightPosition(-1);
			break;
		}

		if (m_inIsleWorld) {
			LegoWorld* world = CurrentWorld();
			if (world && world->GetWorldId() == LegoOmni::e_act1) {
				((Isle*) world)->UpdateGlobe();
			}
		}
	}
}
