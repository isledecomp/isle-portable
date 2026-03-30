#pragma once

#include "extensions/common/constants.h"

#include <cstdint>

class LegoPathActor;

namespace Extensions
{
namespace Common
{

// Animation and vehicle tables (defined in charactertables.cpp)
extern const char* const g_walkAnimNames[];
extern const int g_walkAnimCount;

extern const char* const g_idleAnimNames[];
extern const int g_idleAnimCount;

extern const char* const g_vehicleROINames[VEHICLE_COUNT];
extern const char* const g_rideAnimNames[VEHICLE_COUNT];
extern const char* const g_rideVehicleROINames[VEHICLE_COUNT];

// Returns true if the vehicle type has no ride animation (model swap instead)
bool IsLargeVehicle(int8_t p_vehicleType);

// Detect the vehicle type of a given actor, or VEHICLE_NONE if not a vehicle
int8_t DetectVehicleType(LegoPathActor* p_actor);

} // namespace Common
} // namespace Extensions
