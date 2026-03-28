#pragma once

#include <cstdint>
#include <map>
#include <vector>

class LegoAnimationManager;
class LegoROI;
struct AnimInfo;

namespace Multiplayer::Animation
{

enum AnimCategory : uint8_t {
	e_npcAnim,  // has named character performer && location == -1
	e_camAnim,  // has named character performer && location >= 0
	e_otherAnim // no named character performers (ambient/prop-only)
};

// Number of core playable characters (Pepper, Mama, Papa, Nick, Laura) = g_actorInfoInit indices 0-4
static const int8_t CORE_CHARACTER_COUNT = 5;

// Spectator mask with all core characters enabled
static const uint8_t ALL_CORE_ACTORS_MASK = (1 << CORE_CHARACTER_COUNT) - 1;

// Sentinel value for "no animation selected"
static const uint16_t ANIM_INDEX_NONE = 0xFFFF;

// Extract the character indices from a performer bitmask.
std::vector<int8_t> GetPerformerIndices(uint64_t p_performerMask);

struct CatalogEntry {
	uint16_t animIndex; // Index into LegoAnimationManager::m_anims[]
	AnimCategory category;
	uint8_t spectatorMask;  // Which core actors can trigger (bit0=Pepper..bit4=Laura)
	uint64_t performerMask; // Bitmask of g_actorInfoInit[] indices that appear as character models
	int16_t location;       // -1 = anywhere, >= 0 = specific location
	uint8_t modelCount;     // Number of models in animation
	uint8_t vehicleMask;    // Bitmask of g_vehicles[] indices required (bit0=bikebd..bit6=board)
};

class Catalog {
public:
	void Refresh(LegoAnimationManager* p_am);

	const AnimInfo* GetAnimInfo(uint16_t p_animIndex) const;
	const CatalogEntry* FindEntry(uint16_t p_animIndex) const;

	// All non-otherAnim entries at a location (-1 = NPC anims, >= 0 = location-bound)
	std::vector<const CatalogEntry*> GetAnimationsAtLocation(int16_t p_location) const;

	// Check if a player can fill any role (spectator or participant) in this animation.
	// Accepts a display actor index (converted to g_actorInfoInit index internally).
	bool CanParticipate(const CatalogEntry* p_entry, uint8_t p_displayActorIndex) const;

	// Same check but using a g_actorInfoInit index directly.
	static bool CanParticipateChar(const CatalogEntry* p_entry, int8_t p_charIndex);

	// Check if a set of character indices can collectively trigger this animation.
	// p_onVehicle: parallel array indicating if each player is riding their vehicle (nullable).
	// p_filledPerformers: bitmask of which performer bits in performerMask are covered.
	// p_spectatorFilled: whether a valid spectator was found among unassigned players.
	bool CanTrigger(
		const CatalogEntry* p_entry,
		const int8_t* p_charIndices,
		const uint8_t* p_onVehicle,
		uint8_t p_count,
		uint64_t* p_filledPerformers,
		bool* p_spectatorFilled
	) const;

	// Check if the spectator mask allows this character to spectate.
	// Does NOT check performer exclusion — caller must do that if needed.
	static bool CheckSpectatorMask(const CatalogEntry* p_entry, int8_t p_charIndex);

	// Vehicle riding state for eligibility checks.
	enum VehicleState : uint8_t {
		e_onFoot = 0,         // Not riding anything
		e_onOwnVehicle = 1,   // Riding character's own vehicle (e.g. Pepper on skateboard)
		e_onOtherVehicle = 2  // Riding a vehicle that isn't the character's own
	};

	// Check if a player's vehicle state is compatible with the animation's vehicle requirements.
	static bool CheckVehicleEligibility(const CatalogEntry* p_entry, int8_t p_charIndex, uint8_t p_vehicleState);

	// Determine the vehicle state for a character given their current ride vehicle ROI.
	static VehicleState GetVehicleState(int8_t p_charIndex, LegoROI* p_vehicleROI);

	// Classify a g_vehicles[] index into a vehicle category.
	// Returns 0=bike, 1=motorcycle, 2=skateboard, -1=invalid.
	static int8_t GetVehicleCategory(int8_t p_vehicleIdx);

	// Convert a display actor index to the g_actorInfoInit[] index used by animations.
	// Returns -1 if no match.
	static int8_t DisplayActorToCharacterIndex(uint8_t p_displayActorIndex);

private:
	std::vector<CatalogEntry> m_entries;
	std::map<int16_t, std::vector<size_t>> m_locationIndex; // location ID → indices into m_entries
	AnimInfo* m_animsBase;
	uint16_t m_animCount;
};

} // namespace Multiplayer::Animation
