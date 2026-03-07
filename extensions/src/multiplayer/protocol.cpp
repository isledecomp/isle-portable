#include "extensions/multiplayer/protocol.h"

#include "legopathactor.h"

#include <cstddef>

namespace Multiplayer
{

const char* const g_walkAnimNames[] = {
	"CNs001xx", // 0: Normal (default)
	"CNs002xx", // 1: Joyful
	"CNs003xx", // 2: Gloomy
	"CNs005xx", // 3: Leaning
	"CNs006xx", // 4: Scared
	"CNs007xx", // 5: Hyper
};
const int g_walkAnimCount = sizeof(g_walkAnimNames) / sizeof(g_walkAnimNames[0]);

const char* const g_idleAnimNames[] = {
	"CNs008xx", // 0: Sway (default)
	"CNs009xx", // 1: Groove
	"CNs010xx", // 2: Excited
};
const int g_idleAnimCount = sizeof(g_idleAnimNames) / sizeof(g_idleAnimNames[0]);

const char* const g_emoteAnimNames[] = {
	"CNs011xx", // 0: Wave
	"CNs012xx", // 1: Hat Tip
};
const int g_emoteAnimCount = sizeof(g_emoteAnimNames) / sizeof(g_emoteAnimNames[0]);

// Vehicle model names (LOD names). The helicopter is a compound ROI ("copter")
// with no standalone LOD; use its body part instead.
const char* const g_vehicleROINames[VEHICLE_COUNT] =
	{"chtrbody", "jsuser", "dunebugy", "bike", "board", "moto", "towtk", "ambul"};

// Ride animation names for small vehicles (NULL = large vehicle, no ride anim)
const char* const g_rideAnimNames[VEHICLE_COUNT] =
	{NULL, NULL, NULL, "CNs001Bd", "CNs001sk", "CNs011Ni", NULL, NULL};

// Vehicle variant ROI names used in ride animations
const char* const g_rideVehicleROINames[VEHICLE_COUNT] =
	{NULL, NULL, NULL, "bikebd", "board", "motoni", NULL, NULL};

bool IsLargeVehicle(int8_t p_vehicleType)
{
	return p_vehicleType != VEHICLE_NONE && p_vehicleType < VEHICLE_COUNT && g_rideAnimNames[p_vehicleType] == NULL;
}

int8_t DetectVehicleType(LegoPathActor* p_actor)
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

	if (!p_actor) {
		return VEHICLE_NONE;
	}

	for (const auto& entry : vehicleMap) {
		if (p_actor->IsA(entry.className)) {
			return entry.vehicleType;
		}
	}
	return VEHICLE_NONE;
}

} // namespace Multiplayer
