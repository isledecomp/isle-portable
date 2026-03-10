#include "extensions/multiplayer/protocol.h"

#include "legogamestate.h"
#include "legopathactor.h"
#include "misc.h"

#include <SDL3/SDL_stdinc.h>

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

// Emote animation table. Each entry is {phase1, phase2}.
// phase2 is nullptr for one-shot emotes; non-null makes it a multi-part stateful emote.
const char* const g_emoteAnims[][2] = {
	{"CNs011xx", nullptr},   // 0: Wave (one-shot)
	{"CNs012xx", nullptr},   // 1: Hat Tip (one-shot)
	{"BNsDis01", "BNsAss01"}, // 2: Disassemble / Reassemble (multi-part)
};
const int g_emoteAnimCount = sizeof(g_emoteAnims) / sizeof(g_emoteAnims[0]);

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

void EncodeUsername(char p_out[8])
{
	SDL_memset(p_out, 0, 8);
	LegoGameState* gs = GameState();
	if (gs && gs->m_playerCount > 0) {
		const LegoGameState::Username& username = gs->m_players[0];
		for (int i = 0; i < 7; i++) {
			MxS16 letter = username.m_letters[i];
			if (letter < 0) {
				break;
			}
			if (letter <= 25) {
				p_out[i] = (char) ('A' + letter);
			}
			else {
				p_out[i] = '?';
			}
		}
	}
}

} // namespace Multiplayer
