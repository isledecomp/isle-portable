#pragma once

#include "extensions/common/constants.h"
#include "extensions/multiplayer/networktransport.h"

#include <SDL3/SDL_stdinc.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Multiplayer
{

static constexpr size_t USERNAME_BUFFER_SIZE = 8; // 7 chars + null terminator

// Routing target constants for MessageHeader.target
const uint32_t TARGET_BROADCAST = 0;              // Broadcast to all except sender
const uint32_t TARGET_HOST = 0xFFFFFFFF;          // Send to host only
const uint32_t TARGET_BROADCAST_ALL = 0xFFFFFFFE; // Broadcast to all including sender

enum MessageType : uint8_t {
	MSG_LEAVE = 2,
	MSG_STATE = 3,
	MSG_HOST_ASSIGN = 4,
	MSG_REQUEST_SNAPSHOT = 5,
	MSG_WORLD_SNAPSHOT = 6,
	MSG_WORLD_EVENT = 7,
	MSG_WORLD_EVENT_REQUEST = 8,
	MSG_EMOTE = 9,
	MSG_CUSTOMIZE = 10,
	MSG_ANIM_INTEREST = 11,
	MSG_ANIM_CANCEL = 12,
	MSG_ANIM_UPDATE = 13,
	MSG_ANIM_START = 14,
	MSG_HORN = 16,
	MSG_ASSIGN_ID = 0xFF
};

using Extensions::Common::VEHICLE_AMBULANCE;
using Extensions::Common::VEHICLE_BIKE;
using Extensions::Common::VEHICLE_COUNT;
using Extensions::Common::VEHICLE_DUNEBUGGY;
using Extensions::Common::VEHICLE_HELICOPTER;
using Extensions::Common::VEHICLE_JETSKI;
using Extensions::Common::VEHICLE_MOTOCYCLE;
using Extensions::Common::VEHICLE_NONE;
using Extensions::Common::VEHICLE_SKATEBOARD;
using Extensions::Common::VEHICLE_TOWTRACK;
using Extensions::Common::VehicleType;

// Entity types for world events
enum WorldEntityType : uint8_t {
	ENTITY_PLANT = 0,
	ENTITY_BUILDING = 1,
	ENTITY_SKY = 2,
	ENTITY_LIGHT = 3
};

using Extensions::Common::CHANGE_COLOR;
using Extensions::Common::CHANGE_DECREMENT;
using Extensions::Common::CHANGE_MOOD;
using Extensions::Common::CHANGE_MOVE;
using Extensions::Common::CHANGE_SOUND;
using Extensions::Common::CHANGE_VARIANT;
using Extensions::Common::WorldChangeType;

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
	uint8_t _pad;
	uint32_t peerId;
	uint32_t sequence;
	uint32_t target;
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
	uint8_t walkAnimId;              // Index into walk animation table (0 = default)
	uint8_t idleAnimId;              // Index into idle animation table (0 = default)
	char name[USERNAME_BUFFER_SIZE]; // Player display name (7 chars + null terminator)
	uint8_t displayActorIndex;       // Index into g_actorInfoInit (0-65)
	uint8_t customizeData[5];        // Packed CustomizeState
	uint8_t customizeFlags;          // Bit 0 = allowRemoteCustomize
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
// Relay reads header.target and routes to that peer only.
struct WorldSnapshotMsg {
	MessageHeader header;
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

// One-shot horn sound trigger, broadcast to all peers
struct HornMsg {
	MessageHeader header;
	uint8_t vehicleType; // VehicleType enum value
};

// Immediate customization change, broadcast to all peers
struct CustomizeMsg {
	MessageHeader header;
	uint32_t targetPeerId; // Who is being customized
	uint8_t changeType;    // WorldChangeType (VARIANT/SOUND/MOVE/COLOR/MOOD)
	uint8_t partIndex;     // Body part for color changes (0-9), 0xFF otherwise
};

// Client -> Host: express interest in an animation slot
struct AnimInterestMsg {
	MessageHeader header;
	uint16_t animIndex;
	uint8_t displayActorIndex;
};

// Client -> Host: cancel interest in current animation
struct AnimCancelMsg {
	MessageHeader header;
};

// Per-slot assignment in AnimUpdateMsg
struct AnimSlotAssignment {
	uint32_t peerId; // 0 = unfilled
};

// Host -> All: authoritative session state update
struct AnimUpdateMsg {
	MessageHeader header;
	uint16_t animIndex;
	uint8_t state;               // CoordinationState (0=cleared, 1=gathering, 2=countdown, 3=playing)
	uint16_t countdownMs;        // Remaining countdown ms (0 if not counting)
	uint8_t slotCount;           // Number of valid slot entries
	AnimSlotAssignment slots[8]; // peerId per slot (0 = unfilled)
};

// Host -> All: animation playback trigger
struct AnimStartMsg {
	MessageHeader header;
	uint16_t animIndex;
	uint64_t eventId; // Random 64-bit ID for the completion event (host-generated, same for all clients)
};

#pragma pack(pop)

// Bitmask constants for PlayerStateMsg::customizeFlags
static constexpr uint8_t CUSTOMIZE_FLAG_ALLOW_REMOTE = 0x01;
static constexpr uint8_t CUSTOMIZE_FLAG_FROZEN = 0x02;
static constexpr uint8_t CUSTOMIZE_FLAG_FROZEN_EMOTE_SHIFT = 2;
static constexpr uint8_t CUSTOMIZE_FLAG_FROZEN_EMOTE_MASK = 0x07;

using Extensions::Common::IsValidActorId;

// Convert LegoGameState::Username letter indices (0-25 = A-Z) to ASCII.
// Writes up to 7 characters + null terminator into p_out (must be at least 8 bytes).
void EncodeUsername(char p_out[USERNAME_BUFFER_SIZE]);

using Extensions::Common::DISPLAY_ACTOR_NONE;

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

// Serialize and send a fixed-size message via the transport.
template <typename T>
inline void SendFixedMessage(NetworkTransport* p_transport, const T& p_msg)
{
	if (!p_transport || !p_transport->IsConnected()) {
		return;
	}

	uint8_t buf[sizeof(T)];
	size_t len = SerializeMsg(buf, sizeof(buf), p_msg);
	if (len > 0) {
		p_transport->Send(buf, len);
	}
}

} // namespace Multiplayer
