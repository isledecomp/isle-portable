#pragma once

#include <SDL3/SDL_stdinc.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>

class LegoPathActor;

namespace Multiplayer
{

enum MessageType : uint8_t {
	MSG_JOIN = 1,
	MSG_LEAVE = 2,
	MSG_STATE = 3,
	MSG_HOST_ASSIGN = 4,
	MSG_REQUEST_SNAPSHOT = 5,
	MSG_WORLD_SNAPSHOT = 6,
	MSG_WORLD_EVENT = 7,
	MSG_WORLD_EVENT_REQUEST = 8,
	MSG_EMOTE = 9,
	MSG_CUSTOMIZE = 10,
	MSG_ASSIGN_ID = 0xFF
};

enum VehicleType : int8_t {
	VEHICLE_NONE = -1,
	VEHICLE_HELICOPTER = 0,
	VEHICLE_JETSKI = 1,
	VEHICLE_DUNEBUGGY = 2,
	VEHICLE_BIKE = 3,
	VEHICLE_SKATEBOARD = 4,
	VEHICLE_MOTOCYCLE = 5,
	VEHICLE_TOWTRACK = 6,
	VEHICLE_AMBULANCE = 7,
	VEHICLE_COUNT = 8
};

// Entity types for world events
enum WorldEntityType : uint8_t {
	ENTITY_PLANT = 0,
	ENTITY_BUILDING = 1,
	ENTITY_SKY = 2,
	ENTITY_LIGHT = 3
};

// Change types for world events (maps to Switch* methods on LegoEntity)
enum WorldChangeType : uint8_t {
	CHANGE_VARIANT = 0,
	CHANGE_SOUND = 1,
	CHANGE_MOVE = 2,
	CHANGE_COLOR = 3,
	CHANGE_MOOD = 4,
	CHANGE_DECREMENT = 5
};

// Change types for ENTITY_SKY
enum SkyChangeType : uint8_t {
	SKY_TOGGLE_COLOR = 0,
	SKY_DAY = 1,
	SKY_NIGHT = 2
};

// Change types for ENTITY_LIGHT
enum LightChangeType : uint8_t {
	LIGHT_INCREMENT = 0,
	LIGHT_DECREMENT = 1
};

#pragma pack(push, 1)

struct MessageHeader {
	uint8_t type;
	uint32_t peerId;
	uint32_t sequence;
};

struct PlayerJoinMsg {
	MessageHeader header;
	uint8_t actorId;
	char name[20];
};

struct PlayerLeaveMsg {
	MessageHeader header;
};

struct PlayerStateMsg {
	MessageHeader header;
	uint8_t actorId;
	int8_t worldId;
	int8_t vehicleType;
	float position[3];
	float direction[3];
	float up[3];
	float speed;
	uint8_t walkAnimId; // Index into walk animation table (0 = default)
	uint8_t idleAnimId; // Index into idle animation table (0 = default)
	char name[8];       // Player display name (7 chars + null terminator)
	uint8_t displayActorIndex; // Index into g_actorInfoInit (0-65)
	uint8_t customizeData[5]; // Packed CustomizeState
	uint8_t customizeFlags;   // Bit 0 = allowRemoteCustomize
};

// Server -> all: announces which peer is the host
struct HostAssignMsg {
	MessageHeader header;
	uint32_t hostPeerId;
};

// Client -> host: request full world state snapshot
struct RequestSnapshotMsg {
	MessageHeader header;
};

// Host -> specific client: full world state blob (variable length)
// Relay reads targetPeerId at offset 9 and routes to that peer only.
struct WorldSnapshotMsg {
	MessageHeader header;
	uint32_t targetPeerId;
	uint16_t dataLength;
	// Followed by dataLength bytes of serialized plant + building state
};

// Host -> all: single world state mutation
struct WorldEventMsg {
	MessageHeader header;
	uint8_t entityType;  // WorldEntityType
	uint8_t changeType;  // WorldChangeType
	uint8_t entityIndex; // Index into g_plantInfo[] or g_buildingInfo[]
	uint8_t padding;     // Alignment
};

// Non-host -> host: request a mutation (same layout as WorldEventMsg)
struct WorldEventRequestMsg {
	MessageHeader header;
	uint8_t entityType;  // WorldEntityType
	uint8_t changeType;  // WorldChangeType
	uint8_t entityIndex; // Index into g_plantInfo[] or g_buildingInfo[]
	uint8_t padding;     // Alignment
};

// One-shot emote trigger, broadcast to all peers
struct EmoteMsg {
	MessageHeader header;
	uint8_t emoteId; // Index into emote table
};

// Immediate customization change, broadcast to all peers
struct CustomizeMsg {
	MessageHeader header;
	uint32_t targetPeerId; // Who is being customized
	uint8_t changeType;    // WorldChangeType (VARIANT/SOUND/MOVE/COLOR/MOOD)
	uint8_t partIndex;     // Body part for color changes (0-9), 0xFF otherwise
};

#pragma pack(pop)

// Animation and vehicle tables (defined in protocol.cpp)
extern const char* const g_walkAnimNames[];
extern const int g_walkAnimCount;

extern const char* const g_idleAnimNames[];
extern const int g_idleAnimCount;

extern const char* const g_emoteAnimNames[];
extern const int g_emoteAnimCount;

extern const char* const g_vehicleROINames[VEHICLE_COUNT];
extern const char* const g_rideAnimNames[VEHICLE_COUNT];
extern const char* const g_rideVehicleROINames[VEHICLE_COUNT];

// Returns true if the vehicle type has no ride animation (model swap instead)
bool IsLargeVehicle(int8_t p_vehicleType);

// Detect the vehicle type of a given actor, or VEHICLE_NONE if not a vehicle
int8_t DetectVehicleType(LegoPathActor* p_actor);

// Validate actorId is a playable character (1-5, not brickster)
inline bool IsValidActorId(uint8_t p_actorId)
{
	return p_actorId >= 1 && p_actorId <= 5;
}

static const uint8_t DISPLAY_ACTOR_NONE = 0xFF;

// Parse the message type from a buffer. Returns MSG type or 0 on error.
inline uint8_t ParseMessageType(const uint8_t* p_data, size_t p_length)
{
	if (p_length < 1) {
		return 0;
	}
	return p_data[0];
}

// Generic serialization: copy a packed message struct into a buffer.
template <typename T>
inline size_t SerializeMsg(uint8_t* p_buf, size_t p_bufLen, const T& p_msg)
{
	static_assert(std::is_trivially_copyable_v<T>);
	if (p_bufLen < sizeof(T)) {
		return 0;
	}
	SDL_memcpy(p_buf, &p_msg, sizeof(T));
	return sizeof(T);
}

// Generic deserialization: copy raw bytes into a packed message struct.
template <typename T>
inline bool DeserializeMsg(const uint8_t* p_data, size_t p_length, T& p_out)
{
	static_assert(std::is_trivially_copyable_v<T>);
	if (p_length < sizeof(T)) {
		return false;
	}
	SDL_memcpy(&p_out, p_data, sizeof(T));
	return true;
}

} // namespace Multiplayer
