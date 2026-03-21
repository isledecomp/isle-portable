#include "extensions/common/charactertables.h"

#include "legopathactor.h"

namespace Extensions
{
namespace Common
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
	"CNs008Pa", // 3: Wobbly
	"CNs009Pa", // 4: Peppy
	"CNs012Br", // 5: Brickster
};
const int g_idleAnimCount = sizeof(g_idleAnimNames) / sizeof(g_idleAnimNames[0]);

// Emote table. Each entry has two phases: {anim, sound}.
// Phase 2 anim is nullptr for one-shot emotes; non-null makes it a multi-part stateful emote.
const EmoteEntry g_emoteEntries[] = {
	{{{"CNs011xx", nullptr}, {nullptr, nullptr}}},     // 0: Wave (one-shot)
	{{{"CNs012xx", nullptr}, {nullptr, nullptr}}},     // 1: Hat Tip (one-shot)
	{{{"BNsDis01", "crash5"}, {"BNsAss01", nullptr}}}, // 2: Disassemble / Reassemble (multi-part)
	{{{"CNs008Br", nullptr}, {nullptr, nullptr}}},     // 3: Look Around (one-shot)
	{{{"CNs014Br", nullptr}, {nullptr, nullptr}}},     // 4: Headless (one-shot)
	{{{"CNs013Pa", nullptr}, {nullptr, nullptr}}},     // 5: Toss (one-shot)
};
const int g_emoteAnimCount = sizeof(g_emoteEntries) / sizeof(g_emoteEntries[0]);

// Vehicle model names (LOD names). The helicopter is a compound ROI ("copter")
// with no standalone LOD; use its body part instead.
const char* const g_vehicleROINames[VEHICLE_COUNT] =
	{"chtrbody", "jsuser", "dunebugy", "bike", "board", "moto", "towtk", "ambul"};

// Ride animation names for small vehicles (NULL = large vehicle, no ride anim)
const char* const g_rideAnimNames[VEHICLE_COUNT] = {NULL, NULL, NULL, "CNs001Bd", "CNs001sk", "CNs011Ni", NULL, NULL};

// Vehicle variant ROI names used in ride animations
const char* const g_rideVehicleROINames[VEHICLE_COUNT] = {NULL, NULL, NULL, "bikebd", "board", "motoni", NULL, NULL};

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

} // namespace Common
} // namespace Extensions
