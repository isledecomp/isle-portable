#pragma once

#include <cstdint>

namespace Extensions
{
namespace Common
{

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

// Change types for world events (maps to Switch* methods on LegoEntity)
enum WorldChangeType : uint8_t {
	CHANGE_VARIANT = 0,
	CHANGE_SOUND = 1,
	CHANGE_MOVE = 2,
	CHANGE_COLOR = 3,
	CHANGE_MOOD = 4,
	CHANGE_DECREMENT = 5
};

static const uint8_t DISPLAY_ACTOR_NONE = 0xFF;

// Validate actorId is a playable character (1-5, not brickster)
inline bool IsValidActorId(uint8_t p_actorId)
{
	return p_actorId >= 1 && p_actorId <= 5;
}

} // namespace Common
} // namespace Extensions
