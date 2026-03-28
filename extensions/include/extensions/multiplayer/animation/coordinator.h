#pragma once

#include <cstdint>
#include <map>
#include <vector>

namespace Multiplayer::Animation
{

class Catalog;
struct CatalogEntry;

enum class CoordinationState : uint8_t {
	e_idle,
	e_interested,
	e_countdown,
	e_playing
};

struct SlotInfo {
	// Character names that can fill this slot.
	// Performer slots: always 1 name (the specific character).
	// Spectator slot: ["any"] if ALL_CORE_ACTORS_MASK, otherwise the specific allowed names.
	std::vector<const char*> names;
	bool filled;
};

struct EligibilityInfo {
	uint16_t animIndex;
	bool eligible;               // All requirements met: at location and all roles filled
	bool atLocation;             // At the right location (or location == -1)
	const CatalogEntry* entry;   // Pointer into catalog (valid until next Refresh)
	std::vector<SlotInfo> slots; // All role slots (performers + spectator), filled status each
};

struct SessionView {
	CoordinationState state;
	uint16_t countdownMs;
	uint32_t countdownEndTime; // SDL_GetTicks() timestamp when countdown expires (client-side)
	uint32_t peerSlots[8];     // peerId per slot (matches AnimUpdateMsg layout)
	uint8_t slotCount;
};

class Coordinator {
public:
	Coordinator();

	void SetCatalog(const Catalog* p_catalog);

	CoordinationState GetState() const { return m_state; }
	uint16_t GetCurrentAnimIndex() const { return m_currentAnimIndex; }

	void SetLocalPeerId(uint32_t p_peerId);
	void SetInterest(uint16_t p_animIndex);
	void ClearInterest();

	// Compute eligibility for animations at a location.
	// p_locationChars: local player + remote players at the same location (for cam anims).
	// p_proximityChars: local player + remote players within proximity (for NPC anims).
	// p_locationVehicles/p_proximityVehicles: parallel arrays of VehicleState values.
	std::vector<EligibilityInfo> ComputeEligibility(
		int16_t p_location,
		const int8_t* p_locationChars,
		const uint8_t* p_locationVehicles,
		uint8_t p_locationCount,
		const int8_t* p_proximityChars,
		const uint8_t* p_proximityVehicles,
		uint8_t p_proximityCount
	) const;

	// Auto-clear interest if current animation is not available at any of the new locations.
	void OnLocationChanged(const std::vector<int16_t>& p_locations, const Catalog* p_catalog);

	void Reset();
	void ResetLocalState();
	void RemoveSession(uint16_t p_animIndex);

	// Apply authoritative session state from host
	void ApplySessionUpdate(
		uint16_t p_animIndex,
		uint8_t p_state,
		uint16_t p_countdownMs,
		const uint32_t p_slots[8],
		uint8_t p_slotCount
	);

	// Apply animation start from host
	void ApplyAnimStart(uint16_t p_animIndex);

	// Get session view for an animation (nullptr if no session)
	const SessionView* GetSessionView(uint16_t p_animIndex) const;

	// Check if local player is in a session for this animation
	bool IsLocalPlayerInSession(uint16_t p_animIndex) const;

private:
	const Catalog* m_catalog;
	CoordinationState m_state;
	uint16_t m_currentAnimIndex;
	uint32_t m_localPeerId;

	// When true, a cancel has been sent to the host but not yet confirmed.
	// Prevents stale session updates from re-enrolling the local player.
	bool m_cancelPending;

	// Known sessions from host broadcasts
	std::map<uint16_t, SessionView> m_sessions;
};

} // namespace Multiplayer::Animation
