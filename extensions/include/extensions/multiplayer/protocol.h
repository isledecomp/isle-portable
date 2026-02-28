#pragma once

#include <SDL3/SDL_stdinc.h>
#include <cstddef>
#include <cstdint>

namespace Multiplayer
{

enum MessageType : uint8_t {
	MSG_JOIN = 1,
	MSG_LEAVE = 2,
	MSG_STATE = 3,
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
};

#pragma pack(pop)

static const size_t STATE_MSG_SIZE = sizeof(PlayerStateMsg);
static const size_t JOIN_MSG_SIZE = sizeof(PlayerJoinMsg);
static const size_t LEAVE_MSG_SIZE = sizeof(PlayerLeaveMsg);
static const size_t HEADER_SIZE = sizeof(MessageHeader);

// Validate actorId is a playable character (1-5, not brickster)
inline bool IsValidActorId(uint8_t p_actorId)
{
	return p_actorId >= 1 && p_actorId <= 5;
}

// Serialize a STATE message into a buffer. Returns bytes written.
inline size_t SerializeStateMsg(
	uint8_t* p_buf,
	size_t p_bufLen,
	uint32_t p_peerId,
	uint32_t p_sequence,
	uint8_t p_actorId,
	int8_t p_worldId,
	int8_t p_vehicleType,
	const float p_position[3],
	const float p_direction[3],
	const float p_up[3],
	float p_speed
)
{
	if (p_bufLen < STATE_MSG_SIZE) {
		return 0;
	}

	PlayerStateMsg msg;
	msg.header.type = MSG_STATE;
	msg.header.peerId = p_peerId;
	msg.header.sequence = p_sequence;
	msg.actorId = p_actorId;
	msg.worldId = p_worldId;
	msg.vehicleType = p_vehicleType;
	SDL_memcpy(msg.position, p_position, sizeof(float) * 3);
	SDL_memcpy(msg.direction, p_direction, sizeof(float) * 3);
	SDL_memcpy(msg.up, p_up, sizeof(float) * 3);
	msg.speed = p_speed;

	SDL_memcpy(p_buf, &msg, STATE_MSG_SIZE);
	return STATE_MSG_SIZE;
}

// Serialize a JOIN message into a buffer. Returns bytes written.
inline size_t SerializeJoinMsg(
	uint8_t* p_buf,
	size_t p_bufLen,
	uint32_t p_peerId,
	uint32_t p_sequence,
	uint8_t p_actorId,
	const char* p_name
)
{
	if (p_bufLen < JOIN_MSG_SIZE) {
		return 0;
	}

	PlayerJoinMsg msg;
	msg.header.type = MSG_JOIN;
	msg.header.peerId = p_peerId;
	msg.header.sequence = p_sequence;
	msg.actorId = p_actorId;
	SDL_memset(msg.name, 0, sizeof(msg.name));
	if (p_name) {
		SDL_strlcpy(msg.name, p_name, sizeof(msg.name));
	}

	SDL_memcpy(p_buf, &msg, JOIN_MSG_SIZE);
	return JOIN_MSG_SIZE;
}

// Serialize a LEAVE message into a buffer. Returns bytes written.
inline size_t SerializeLeaveMsg(uint8_t* p_buf, size_t p_bufLen, uint32_t p_peerId, uint32_t p_sequence)
{
	if (p_bufLen < LEAVE_MSG_SIZE) {
		return 0;
	}

	PlayerLeaveMsg msg;
	msg.header.type = MSG_LEAVE;
	msg.header.peerId = p_peerId;
	msg.header.sequence = p_sequence;

	SDL_memcpy(p_buf, &msg, LEAVE_MSG_SIZE);
	return LEAVE_MSG_SIZE;
}

// Parse the message type from a buffer. Returns MSG type or 0 on error.
inline uint8_t ParseMessageType(const uint8_t* p_data, size_t p_length)
{
	if (p_length < 1) {
		return 0;
	}
	return p_data[0];
}

// Deserialize a message header from a buffer.
inline bool DeserializeHeader(const uint8_t* p_data, size_t p_length, MessageHeader& p_out)
{
	if (p_length < HEADER_SIZE) {
		return false;
	}
	SDL_memcpy(&p_out, p_data, HEADER_SIZE);
	return true;
}

// Deserialize a STATE message from a buffer.
inline bool DeserializeStateMsg(const uint8_t* p_data, size_t p_length, PlayerStateMsg& p_out)
{
	if (p_length < STATE_MSG_SIZE) {
		return false;
	}
	SDL_memcpy(&p_out, p_data, STATE_MSG_SIZE);
	return p_out.header.type == MSG_STATE;
}

// Deserialize a JOIN message from a buffer.
inline bool DeserializeJoinMsg(const uint8_t* p_data, size_t p_length, PlayerJoinMsg& p_out)
{
	if (p_length < JOIN_MSG_SIZE) {
		return false;
	}
	SDL_memcpy(&p_out, p_data, JOIN_MSG_SIZE);
	p_out.name[sizeof(p_out.name) - 1] = '\0';
	return p_out.header.type == MSG_JOIN && IsValidActorId(p_out.actorId);
}

// Deserialize a LEAVE message from a buffer.
inline bool DeserializeLeaveMsg(const uint8_t* p_data, size_t p_length, PlayerLeaveMsg& p_out)
{
	if (p_length < LEAVE_MSG_SIZE) {
		return false;
	}
	SDL_memcpy(&p_out, p_data, LEAVE_MSG_SIZE);
	return p_out.header.type == MSG_LEAVE;
}

} // namespace Multiplayer
