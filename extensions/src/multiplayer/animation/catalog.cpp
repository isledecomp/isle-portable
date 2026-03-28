#include "extensions/multiplayer/animation/catalog.h"

#include "actions/isle_actions.h"
#include "decomp.h"
#include "legoactors.h"
#include "legoanimationmanager.h"
#include "misc.h"
#include "roi/legoroi.h"

#include <SDL3/SDL_stdinc.h>

using namespace Multiplayer::Animation;

// Defined in legoanimationmanager.cpp — not exported in headers.
extern LegoAnimationManager::Character g_characters[47];
extern LegoAnimationManager::Vehicle g_vehicles[7];

// Look up the g_vehicles[] index for a character's owned vehicle.
// p_actorInfoIndex is an index into g_actorInfoInit[].
// Returns -1 if the character has no vehicle.
static int8_t GetCharacterVehicleId(int8_t p_actorInfoIndex)
{
	if (p_actorInfoIndex < 0 || p_actorInfoIndex >= (int8_t) SDL_min(sizeOfArray(g_actorInfoInit), (size_t) 64)) {
		return -1;
	}
	const char* name = g_actorInfoInit[p_actorInfoIndex].m_name;
	if (!name) {
		return -1;
	}
	for (int i = 0; i < (int) sizeOfArray(g_characters); i++) {
		if (!SDL_strcasecmp(name, g_characters[i].m_name)) {
			return g_characters[i].m_vehicleId;
		}
	}
	return -1;
}

// Exact-match a model name against g_actorInfoInit[].m_name.
// The engine's LegoAnimationManager::GetCharacterIndex uses 2-char prefix matching,
// which causes false positives (e.g. "ladder" matching "laura"). We need exact
// matching to correctly identify character performers vs props.
// Capped at 64 because performerMask is uint64_t.
static int8_t GetCharacterIndex(const char* p_name)
{
	for (int8_t i = 0; i < (int8_t) SDL_min(sizeOfArray(g_actorInfoInit), (size_t) 64); i++) {
		if (!SDL_strcasecmp(p_name, g_actorInfoInit[i].m_name)) {
			return i;
		}
	}
	return -1;
}

std::vector<int8_t> Multiplayer::Animation::GetPerformerIndices(uint64_t p_performerMask)
{
	std::vector<int8_t> indices;
	for (int8_t i = 0; i < 64; i++) {
		if (p_performerMask & (uint64_t(1) << i)) {
			indices.push_back(i);
		}
	}
	return indices;
}

void Catalog::Refresh(LegoAnimationManager* p_am)
{
	m_entries.clear();
	m_locationIndex.clear();
	m_animsBase = nullptr;
	m_animCount = 0;

	if (!p_am) {
		return;
	}

	m_animCount = p_am->m_animCount;
	m_animsBase = p_am->m_anims;

	if (!m_animsBase || m_animCount == 0) {
		return;
	}

	for (uint16_t i = 0; i < m_animCount; i++) {
		if (!m_animsBase[i].m_name || m_animsBase[i].m_objectId == 0) {
			continue;
		}

		CatalogEntry entry;
		entry.animIndex = i;
		entry.spectatorMask = m_animsBase[i].m_unk0x0c;
		entry.location = m_animsBase[i].m_location;
		entry.modelCount = m_animsBase[i].m_modelCount;

		// Compute performerMask by matching models against g_actorInfoInit[].m_name
		entry.performerMask = 0;
		for (uint8_t m = 0; m < entry.modelCount; m++) {
			if (m_animsBase[i].m_models && m_animsBase[i].m_models[m].m_name) {
				int8_t charIdx = GetCharacterIndex(m_animsBase[i].m_models[m].m_name);
				if (charIdx >= 0) {
					entry.performerMask |= (uint64_t(1) << charIdx);
				}
			}
		}

		// Compute vehicleMask from the pre-populated vehicle list (m_unk0x2a).
		// Each entry is a g_vehicles[] index set during LoadWorldInfo for models
		// with m_unk0x2c=1 that match a known vehicle name.
		entry.vehicleMask = 0;
		for (int k = 0; k < 3; k++) {
			if (m_animsBase[i].m_unk0x2a[k] >= 0 && m_animsBase[i].m_unk0x2a[k] < (int8_t) sizeOfArray(g_vehicles)) {
				entry.vehicleMask |= (1 << m_animsBase[i].m_unk0x2a[k]);
			}
		}

		// Categorize based on whether the animation has named character performers.
		// g_actorInfoInit layout:
		//   0-47:  named characters (pepper through jk)
		//   48-53: ghosts
		//   54-57: named characters (hg, pntgy, pep, cop01)
		//   58-65: generic extras (actor_01-05, vehicle riders)
		static const uint64_t NAMED_CHARACTER_MASK = ((uint64_t(1) << 48) - 1) | (uint64_t(0xF) << 54);
		bool hasNamedPerformer = (entry.performerMask & NAMED_CHARACTER_MASK) != 0;

		// Manual overrides for prop-only animations that have no character
		// performers but are valid scene animations with spectator-only slots.
		MxU32 objectId = m_animsBase[i].m_objectId;
		if (objectId == IsleScript::c_snsx31sh_RunAnim || objectId == IsleScript::c_fpz166p1_RunAnim ||
			objectId == IsleScript::c_nic002pr_RunAnim || objectId == IsleScript::c_nic003pr_RunAnim ||
			objectId == IsleScript::c_nic004pr_RunAnim || objectId == IsleScript::c_prp101pr_RunAnim) {
			if (objectId == IsleScript::c_prp101pr_RunAnim) {
				entry.location = 11; // Hospital
			}
			entry.category = e_camAnim;
		}
		else if (!hasNamedPerformer) {
			entry.category = e_otherAnim;
		}
		else if (entry.location == -1) {
			entry.category = e_npcAnim;
		}
		else {
			entry.category = e_camAnim;
		}

		size_t idx = m_entries.size();
		m_entries.push_back(entry);

		// Build location index
		m_locationIndex[entry.location].push_back(idx);
	}
}

const AnimInfo* Catalog::GetAnimInfo(uint16_t p_animIndex) const
{
	if (!m_animsBase || p_animIndex >= m_animCount) {
		return nullptr;
	}
	return &m_animsBase[p_animIndex];
}

int8_t Catalog::DisplayActorToCharacterIndex(uint8_t p_displayActorIndex)
{
	if (p_displayActorIndex >= SDL_min(sizeOfArray(g_actorInfoInit), (size_t) 64)) {
		return -1;
	}
	return static_cast<int8_t>(p_displayActorIndex);
}

const CatalogEntry* Catalog::FindEntry(uint16_t p_animIndex) const
{
	for (const auto& entry : m_entries) {
		if (entry.animIndex == p_animIndex) {
			return &entry;
		}
	}
	return nullptr;
}

std::vector<const CatalogEntry*> Catalog::GetAnimationsAtLocation(int16_t p_location) const
{
	std::vector<const CatalogEntry*> result;

	// Helper to add entries from a location, filtering out e_otherAnim
	auto addFromLocation = [&](int16_t loc) {
		auto it = m_locationIndex.find(loc);
		if (it != m_locationIndex.end()) {
			for (size_t idx : it->second) {
				if (m_entries[idx].category != e_otherAnim) {
					result.push_back(&m_entries[idx]);
				}
			}
		}
	};

	// Always include NPC animations (location == -1)
	addFromLocation(-1);

	// If requesting a specific location, also include location-bound animations
	if (p_location >= 0) {
		addFromLocation(p_location);
	}

	return result;
}

bool Catalog::CheckSpectatorMask(const CatalogEntry* p_entry, int8_t p_charIndex)
{
	if (p_charIndex < CORE_CHARACTER_COUNT) {
		return (p_entry->spectatorMask >> p_charIndex) & 1;
	}

	// Non-core characters (index 5+): only if all core actors allowed
	return p_entry->spectatorMask == ALL_CORE_ACTORS_MASK;
}

bool Catalog::CheckVehicleEligibility(const CatalogEntry* p_entry, int8_t p_charIndex, uint8_t p_vehicleState)
{
	int8_t vehicleId = GetCharacterVehicleId(p_charIndex);
	if (vehicleId < 0) {
		return true; // Character has no vehicle — no constraint (Mama, Papa, NPCs)
	}

	bool animUsesVehicle = (p_entry->vehicleMask >> vehicleId) & 1;

	switch (p_vehicleState) {
	case e_onOwnVehicle:
		return animUsesVehicle; // Only animations that use this character's vehicle
	case e_onOtherVehicle:
		return false; // On a foreign vehicle — no animations eligible
	default: // e_onFoot
		return !animUsesVehicle; // Only animations that don't use this character's vehicle
	}
}

int8_t Catalog::GetVehicleCategory(int8_t p_vehicleIdx)
{
	if (p_vehicleIdx >= 0 && p_vehicleIdx <= 3) {
		return 0; // bike (bikebd, bikepg, bikerd, bikesy)
	}
	if (p_vehicleIdx >= 4 && p_vehicleIdx <= 5) {
		return 1; // motorcycle (motoni, motola)
	}
	if (p_vehicleIdx == 6) {
		return 2; // skateboard (board)
	}
	return -1;
}

Catalog::VehicleState Catalog::GetVehicleState(int8_t p_charIndex, LegoROI* p_vehicleROI)
{
	if (!p_vehicleROI || !p_vehicleROI->GetName()) {
		return e_onFoot;
	}

	int8_t charVehicleId = GetCharacterVehicleId(p_charIndex);
	if (charVehicleId < 0) {
		return e_onFoot; // Character has no vehicle — treat any ride as irrelevant
	}

	MxU32 rideVehicleIdx;
	if (!AnimationManager()->FindVehicle(p_vehicleROI->GetName(), rideVehicleIdx)) {
		return e_onOtherVehicle; // Unknown vehicle — treat as foreign
	}

	// Compare by category — the ride system uses representative names (bikebd/motoni/board)
	// that may differ from the character's specific vehicle index but share the same category.
	if (GetVehicleCategory((int8_t) rideVehicleIdx) == GetVehicleCategory(charVehicleId)) {
		return e_onOwnVehicle;
	}

	return e_onOtherVehicle;
}

bool Catalog::CanParticipateChar(const CatalogEntry* p_entry, int8_t p_charIndex)
{
	if (p_charIndex < 0) {
		return false;
	}

	// Performer: player's character is one of the performing models
	if ((p_entry->performerMask >> p_charIndex) & 1) {
		return true;
	}

	// Spectator: not a performer, spectator mask allows them
	return CheckSpectatorMask(p_entry, p_charIndex);
}

bool Catalog::CanParticipate(const CatalogEntry* p_entry, uint8_t p_displayActorIndex) const
{
	return CanParticipateChar(p_entry, DisplayActorToCharacterIndex(p_displayActorIndex));
}

bool Catalog::CanTrigger(
	const CatalogEntry* p_entry,
	const int8_t* p_charIndices,
	const uint8_t* p_onVehicle,
	uint8_t p_count,
	uint64_t* p_filledPerformers,
	bool* p_spectatorFilled
) const
{
	*p_filledPerformers = 0;
	*p_spectatorFilled = false;

	// First pass: assign performers (each performer slot needs exactly one player)
	std::vector<bool> assignedAsPerformer(p_count, false);

	for (uint8_t i = 0; i < p_count; i++) {
		int8_t charIndex = p_charIndices[i];
		if (charIndex < 0) {
			continue;
		}

		uint64_t charBit = uint64_t(1) << charIndex;
		if ((p_entry->performerMask & charBit) && !(*p_filledPerformers & charBit)) {
			if (p_onVehicle && !CheckVehicleEligibility(p_entry, charIndex, p_onVehicle[i])) {
				continue;
			}

			*p_filledPerformers |= charBit;
			assignedAsPerformer[i] = true;
		}
	}

	bool allPerformersCovered = (*p_filledPerformers == p_entry->performerMask);

	// Second pass: find a spectator among unassigned players
	for (uint8_t i = 0; i < p_count; i++) {
		if (assignedAsPerformer[i]) {
			continue;
		}

		int8_t charIndex = p_charIndices[i];
		if (charIndex >= 0 && !((p_entry->performerMask >> charIndex) & 1) && CheckSpectatorMask(p_entry, charIndex)) {
			*p_spectatorFilled = true;
			break;
		}
	}

	return allPerformersCovered && *p_spectatorFilled;
}
