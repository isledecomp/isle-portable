#pragma once

#include "extensions/multiplayer/protocol.h"

#include <cstdint>

class LegoPathActor;

namespace Multiplayer
{

// Animation and vehicle tables (defined in animdata.cpp)
extern const char* const g_walkAnimNames[];
extern const int g_walkAnimCount;

extern const char* const g_idleAnimNames[];
extern const int g_idleAnimCount;

// Per-phase emote data: animation name and optional sound effect.
struct EmotePhase {
	const char* anim;  // Animation name (nullptr = unused phase)
	const char* sound; // Sound key for LegoCacheSoundManager (nullptr = silent)
};

// Emote table entry: two phases (phase 1 = primary, phase 2 = recovery for multi-part emotes).
struct EmoteEntry {
	EmotePhase phases[2];
};

extern const EmoteEntry g_emoteEntries[];
extern const int g_emoteAnimCount;

// Returns true if the emote is a multi-part stateful emote (has a phase-2 animation).
inline bool IsMultiPartEmote(uint8_t p_emoteId)
{
	return p_emoteId < g_emoteAnimCount && g_emoteEntries[p_emoteId].phases[1].anim != nullptr;
}

extern const char* const g_vehicleROINames[VEHICLE_COUNT];
extern const char* const g_rideAnimNames[VEHICLE_COUNT];
extern const char* const g_rideVehicleROINames[VEHICLE_COUNT];

// Returns true if the vehicle type has no ride animation (model swap instead)
bool IsLargeVehicle(int8_t p_vehicleType);

// Detect the vehicle type of a given actor, or VEHICLE_NONE if not a vehicle
int8_t DetectVehicleType(LegoPathActor* p_actor);

} // namespace Multiplayer
