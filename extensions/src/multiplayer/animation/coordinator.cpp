#include "extensions/multiplayer/animation/coordinator.h"

#include "extensions/multiplayer/animation/catalog.h"
#include "legoactors.h"

#include <SDL3/SDL_timer.h>

using namespace Multiplayer::Animation;

Coordinator::Coordinator()
	: m_catalog(nullptr), m_state(CoordinationState::e_idle), m_currentAnimIndex(ANIM_INDEX_NONE), m_localPeerId(0),
	  m_cancelPending(false)
{
}

void Coordinator::SetCatalog(const Catalog* p_catalog)
{
	m_catalog = p_catalog;
}

void Coordinator::SetLocalPeerId(uint32_t p_peerId)
{
	m_localPeerId = p_peerId;
}

void Coordinator::SetInterest(uint16_t p_animIndex)
{
	if (m_state != CoordinationState::e_idle && m_state != CoordinationState::e_interested) {
		return;
	}

	m_currentAnimIndex = p_animIndex;
	m_state = CoordinationState::e_interested;
	m_cancelPending = false;
}

void Coordinator::ClearInterest()
{
	if (m_state == CoordinationState::e_interested || m_state == CoordinationState::e_countdown ||
		m_state == CoordinationState::e_playing) {
		m_state = CoordinationState::e_idle;
		m_currentAnimIndex = ANIM_INDEX_NONE;
		m_cancelPending = true;
	}
}

// Build the unified slots vector from CanTrigger results.
// Each bit in performerMask becomes one slot; the spectator becomes one slot at the end.
static void BuildSlots(
	const CatalogEntry* p_entry,
	uint64_t p_filledPerformers,
	bool p_spectatorFilled,
	std::vector<SlotInfo>& p_slots
)
{
	// One slot per performer bit in performerMask
	for (int8_t i : GetPerformerIndices(p_entry->performerMask)) {
		SlotInfo slot;
		if (i < (int8_t) sizeOfArray(g_actorInfoInit)) {
			slot.names.push_back(g_actorInfoInit[i].m_name);
		}
		slot.filled = (p_filledPerformers & (uint64_t(1) << i)) != 0;
		p_slots.push_back(std::move(slot));
	}

	// One spectator slot
	SlotInfo spectatorSlot;
	if (p_entry->spectatorMask == ALL_CORE_ACTORS_MASK) {
		spectatorSlot.names.push_back("any");
	}
	else {
		for (int8_t i = 0; i < CORE_CHARACTER_COUNT; i++) {
			if ((p_entry->spectatorMask >> i) & 1) {
				spectatorSlot.names.push_back(g_actorInfoInit[i].m_name);
			}
		}
	}
	spectatorSlot.filled = p_spectatorFilled;
	p_slots.push_back(std::move(spectatorSlot));
}

std::vector<EligibilityInfo> Coordinator::ComputeEligibility(
	int16_t p_location,
	const int8_t* p_locationChars,
	uint8_t p_locationCount,
	const int8_t* p_proximityChars,
	uint8_t p_proximityCount
) const
{
	std::vector<EligibilityInfo> result;

	if (!m_catalog || p_locationCount == 0) {
		return result;
	}

	auto anims = m_catalog->GetAnimationsAtLocation(p_location);

	for (const CatalogEntry* entry : anims) {
		// p_locationChars[0] == p_proximityChars[0] == local player
		if (!Catalog::CanParticipateChar(entry, p_locationChars[0])) {
			continue;
		}

		// NPC anims (location == -1): use proximity characters
		// Cam anims (location >= 0): use location characters
		const int8_t* chars = (entry->location == -1) ? p_proximityChars : p_locationChars;
		uint8_t count = (entry->location == -1) ? p_proximityCount : p_locationCount;

		EligibilityInfo info;
		info.animIndex = entry->animIndex;
		info.entry = entry;

		bool atLoc = (entry->location == -1) || (entry->location == p_location);
		info.atLocation = atLoc;

		uint64_t filledPerformers = 0;
		bool spectatorFilled = false;

		if (atLoc) {
			info.eligible = m_catalog->CanTrigger(entry, chars, count, &filledPerformers, &spectatorFilled);
		}
		else {
			info.eligible = false;
		}

		BuildSlots(entry, filledPerformers, spectatorFilled, info.slots);

		// Override slot fills with authoritative session data
		auto sessionIt = m_sessions.find(entry->animIndex);
		if (sessionIt != m_sessions.end()) {
			const SessionView& sv = sessionIt->second;
			uint8_t slotCount =
				sv.slotCount < info.slots.size() ? sv.slotCount : static_cast<uint8_t>(info.slots.size());
			for (uint8_t s = 0; s < slotCount; s++) {
				info.slots[s].filled = (sv.peerSlots[s] != 0);
			}
		}

		result.push_back(std::move(info));
	}

	return result;
}

void Coordinator::OnLocationChanged(const std::vector<int16_t>& p_locations, const Catalog* p_catalog)
{
	if (m_state != CoordinationState::e_interested || !p_catalog) {
		return;
	}

	// Check if the currently interested animation is still available at any of the locations
	for (int16_t loc : p_locations) {
		auto anims = p_catalog->GetAnimationsAtLocation(loc);
		for (const auto* e : anims) {
			if (e->animIndex == m_currentAnimIndex) {
				return; // still available at this location
			}
		}
	}

	// Also check NPC anims when at no location
	if (p_locations.empty()) {
		auto anims = p_catalog->GetAnimationsAtLocation(-1);
		for (const auto* e : anims) {
			if (e->animIndex == m_currentAnimIndex) {
				return;
			}
		}
	}

	// Animation not at any current location — clear interest
	m_state = CoordinationState::e_idle;
	m_currentAnimIndex = ANIM_INDEX_NONE;
	m_cancelPending = true;
}

void Coordinator::Reset()
{
	m_state = CoordinationState::e_idle;
	m_currentAnimIndex = ANIM_INDEX_NONE;
	m_sessions.clear();
	m_cancelPending = false;
}

void Coordinator::ResetLocalState()
{
	m_state = CoordinationState::e_idle;
	m_currentAnimIndex = ANIM_INDEX_NONE;
	m_cancelPending = false;
}

void Coordinator::RemoveSession(uint16_t p_animIndex)
{
	m_sessions.erase(p_animIndex);
}

void Coordinator::ApplySessionUpdate(
	uint16_t p_animIndex,
	uint8_t p_state,
	uint16_t p_countdownMs,
	const uint32_t p_slots[8],
	uint8_t p_slotCount
)
{
	if (p_state == 0) {
		// Session cleared
		m_sessions.erase(p_animIndex);

		// If local player was in this session, reset to idle
		if (m_currentAnimIndex == p_animIndex &&
			(m_state == CoordinationState::e_interested || m_state == CoordinationState::e_countdown ||
			 m_state == CoordinationState::e_playing)) {
			m_state = CoordinationState::e_idle;
			m_currentAnimIndex = ANIM_INDEX_NONE;
		}
		return;
	}

	SessionView& sv = m_sessions[p_animIndex];
	sv.state = static_cast<CoordinationState>(p_state);
	sv.countdownMs = p_countdownMs;
	sv.countdownEndTime = (p_countdownMs > 0) ? (SDL_GetTicks() + p_countdownMs) : 0;
	sv.slotCount = p_slotCount < 8 ? p_slotCount : 8;
	for (uint8_t i = 0; i < 8; i++) {
		sv.peerSlots[i] = (i < sv.slotCount) ? p_slots[i] : 0;
	}

	// If local player is in this session, update coordinator state
	if (m_localPeerId != 0) {
		bool localInSession = false;
		for (uint8_t i = 0; i < sv.slotCount; i++) {
			if (sv.peerSlots[i] == m_localPeerId) {
				localInSession = true;
				break;
			}
		}

		if (localInSession && !m_cancelPending) {
			m_currentAnimIndex = p_animIndex;
			m_state = sv.state;
		}
		else if (!localInSession) {
			if (m_currentAnimIndex == p_animIndex) {
				m_state = CoordinationState::e_idle;
				m_currentAnimIndex = ANIM_INDEX_NONE;
			}
			m_cancelPending = false;
		}
	}
}

void Coordinator::ApplyAnimStart(uint16_t p_animIndex)
{
	if (IsLocalPlayerInSession(p_animIndex)) {
		m_state = CoordinationState::e_playing;
		m_currentAnimIndex = p_animIndex;
	}

	// Update session view so PushAnimationState reads correct values
	auto it = m_sessions.find(p_animIndex);
	if (it != m_sessions.end()) {
		it->second.state = CoordinationState::e_playing;
		it->second.countdownMs = 0;
	}
}

const SessionView* Coordinator::GetSessionView(uint16_t p_animIndex) const
{
	auto it = m_sessions.find(p_animIndex);
	if (it != m_sessions.end()) {
		return &it->second;
	}
	return nullptr;
}

bool Coordinator::IsLocalPlayerInSession(uint16_t p_animIndex) const
{
	if (m_cancelPending || m_localPeerId == 0) {
		return false;
	}

	auto it = m_sessions.find(p_animIndex);
	if (it == m_sessions.end()) {
		return false;
	}

	for (uint8_t i = 0; i < it->second.slotCount; i++) {
		if (it->second.peerSlots[i] == m_localPeerId) {
			return true;
		}
	}
	return false;
}
