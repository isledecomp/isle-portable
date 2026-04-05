#include "extensions/multiplayer/animation/sessionhost.h"

#include "extensions/multiplayer/animation/catalog.h"
#include "extensions/multiplayer/animation/coordinator.h"

#include <SDL3/SDL_timer.h>

using namespace Multiplayer::Animation;

static bool HasAnyFilledSlot(const AnimSession& p_session)
{
	for (const auto& slot : p_session.slots) {
		if (slot.peerId != 0) {
			return true;
		}
	}
	return false;
}

void SessionHost::SetCatalog(const Catalog* p_catalog)
{
	m_catalog = p_catalog;
}

AnimSession SessionHost::CreateSession(const CatalogEntry* p_entry, uint16_t p_animIndex)
{
	AnimSession session;
	session.animIndex = p_animIndex;
	session.state = CoordinationState::e_interested;
	session.countdownEndTime = 0;

	for (int8_t i : GetPerformerIndices(p_entry->performerMask)) {
		SessionSlot slot;
		slot.peerId = 0;
		slot.charIndex = i;
		session.slots.push_back(slot);
	}

	SessionSlot spectatorSlot;
	spectatorSlot.peerId = 0;
	spectatorSlot.charIndex = -1;
	session.slots.push_back(spectatorSlot);

	return session;
}

bool SessionHost::TryAssignSlot(AnimSession& p_session, uint32_t p_peerId, int8_t p_charIndex)
{
	for (const auto& slot : p_session.slots) {
		if (slot.peerId == p_peerId) {
			return false;
		}
	}

	// Performer slots first
	for (auto& slot : p_session.slots) {
		if (!slot.IsSpectator() && slot.peerId == 0 && slot.charIndex == p_charIndex) {
			slot.peerId = p_peerId;
			return true;
		}
	}

	// Spectator slot
	if (!m_catalog) {
		return false;
	}

	const CatalogEntry* entry = m_catalog->FindEntry(p_session.animIndex);
	if (!entry) {
		return false;
	}

	for (auto& slot : p_session.slots) {
		if (slot.IsSpectator() && slot.peerId == 0) {
			if (p_charIndex >= 0 && !((entry->performerMask >> p_charIndex) & 1) &&
				Catalog::CheckSpectatorMask(entry, p_charIndex)) {
				slot.peerId = p_peerId;
				return true;
			}
			break;
		}
	}

	return false;
}

bool SessionHost::AllSlotsFilled(const AnimSession& p_session) const
{
	for (const auto& slot : p_session.slots) {
		if (slot.peerId == 0) {
			return false;
		}
	}
	return true;
}

void SessionHost::RemovePlayerFromAllSessions(uint32_t p_peerId, std::vector<uint16_t>& p_changedAnims)
{
	RemovePlayerFromSessions(p_peerId, false, p_changedAnims);
}

void SessionHost::RemovePlayerFromSessions(
	uint32_t p_peerId,
	bool p_includePlayingSessions,
	std::vector<uint16_t>& p_changedAnims
)
{
	std::vector<uint16_t> toErase;

	for (auto& [animIndex, session] : m_sessions) {
		if (!p_includePlayingSessions && session.state == CoordinationState::e_playing) {
			continue;
		}

		bool found = false;
		for (auto& slot : session.slots) {
			if (slot.peerId == p_peerId) {
				slot.peerId = 0;
				found = true;
				break;
			}
		}

		if (found) {
			if (session.state == CoordinationState::e_countdown) {
				session.state = CoordinationState::e_interested;
				session.countdownEndTime = 0;
			}

			if (!HasAnyFilledSlot(session)) {
				toErase.push_back(animIndex);
			}

			p_changedAnims.push_back(animIndex);
		}
	}

	for (uint16_t idx : toErase) {
		m_sessions.erase(idx);
	}
}

bool SessionHost::HandleInterest(
	uint32_t p_peerId,
	uint16_t p_animIndex,
	uint8_t p_displayActorIndex,
	std::vector<uint16_t>& p_changedAnims
)
{
	if (!m_catalog) {
		return false;
	}

	int8_t charIndex = Catalog::DisplayActorToCharacterIndex(p_displayActorIndex);

	RemovePlayerFromAllSessions(p_peerId, p_changedAnims);

	const CatalogEntry* entry = m_catalog->FindEntry(p_animIndex);
	if (!entry) {
		return !p_changedAnims.empty();
	}

	auto it = m_sessions.find(p_animIndex);
	if (it == m_sessions.end()) {
		m_sessions[p_animIndex] = CreateSession(entry, p_animIndex);
		it = m_sessions.find(p_animIndex);
	}

	bool assigned = TryAssignSlot(it->second, p_peerId, charIndex);

	// Always broadcast: on success the new slot is shown, on failure the rejected
	// player's client receives the session state and clears their optimistic interest.
	p_changedAnims.push_back(p_animIndex);

	// Clean up empty sessions (created but no one could fill a slot)
	if (!assigned) {
		if (!HasAnyFilledSlot(it->second)) {
			m_sessions.erase(it);
		}
	}

	return !p_changedAnims.empty();
}

bool SessionHost::HandleCancel(uint32_t p_peerId, std::vector<uint16_t>& p_changedAnims)
{
	RemovePlayerFromSessions(p_peerId, true, p_changedAnims);

	// Explicit cancel during playback: erase entire session so all participants stop
	for (uint16_t animIndex : p_changedAnims) {
		auto it = m_sessions.find(animIndex);
		if (it != m_sessions.end() && it->second.state == CoordinationState::e_playing) {
			m_sessions.erase(it);
		}
	}

	return !p_changedAnims.empty();
}

bool SessionHost::HandlePlayerRemoved(uint32_t p_peerId, std::vector<uint16_t>& p_changedAnims)
{
	RemovePlayerFromSessions(p_peerId, true, p_changedAnims);
	return !p_changedAnims.empty();
}

void SessionHost::StartCountdown(uint16_t p_animIndex)
{
	auto it = m_sessions.find(p_animIndex);
	if (it != m_sessions.end() && it->second.state == CoordinationState::e_interested) {
		it->second.state = CoordinationState::e_countdown;
		it->second.countdownEndTime = SDL_GetTicks() + COUNTDOWN_DURATION_MS;
	}
}

void SessionHost::RevertCountdown(uint16_t p_animIndex)
{
	auto it = m_sessions.find(p_animIndex);
	if (it != m_sessions.end() && it->second.state == CoordinationState::e_countdown) {
		it->second.state = CoordinationState::e_interested;
		it->second.countdownEndTime = 0;
	}
}

std::vector<uint16_t> SessionHost::Tick(uint32_t p_now)
{
	std::vector<uint16_t> ready;
	for (auto& [animIndex, session] : m_sessions) {
		if (session.state == CoordinationState::e_countdown && p_now >= session.countdownEndTime) {
			session.state = CoordinationState::e_playing;
			ready.push_back(animIndex);
		}
	}
	return ready;
}

void SessionHost::Reset()
{
	m_sessions.clear();
}

void SessionHost::EraseSession(uint16_t p_animIndex)
{
	m_sessions.erase(p_animIndex);
}

const AnimSession* SessionHost::FindSession(uint16_t p_animIndex) const
{
	auto it = m_sessions.find(p_animIndex);
	if (it != m_sessions.end()) {
		return &it->second;
	}
	return nullptr;
}

const std::map<uint16_t, AnimSession>& SessionHost::GetSessions() const
{
	return m_sessions;
}

bool SessionHost::AreAllSlotsFilled(uint16_t p_animIndex) const
{
	auto it = m_sessions.find(p_animIndex);
	if (it == m_sessions.end()) {
		return false;
	}
	return AllSlotsFilled(it->second);
}

uint16_t SessionHost::ComputeCountdownMs(const AnimSession& p_session, uint32_t p_now)
{
	if (p_session.state != CoordinationState::e_countdown) {
		return 0;
	}

	if (p_now >= p_session.countdownEndTime) {
		return 0;
	}

	uint32_t remaining = p_session.countdownEndTime - p_now;
	if (remaining > 0xFFFF) {
		return 0xFFFF;
	}
	return static_cast<uint16_t>(remaining);
}

bool SessionHost::HasCountdownSession() const
{
	for (const auto& [animIndex, session] : m_sessions) {
		if (session.state == CoordinationState::e_countdown) {
			return true;
		}
	}
	return false;
}

std::vector<int8_t> SessionHost::ComputeSlotCharIndices(const CatalogEntry* p_entry)
{
	std::vector<int8_t> indices;
	if (!p_entry) {
		return indices;
	}

	// Performers: one slot per set bit in performerMask (same order as CreateSession)
	indices = GetPerformerIndices(p_entry->performerMask);

	// Spectator slot last
	indices.push_back(-1);

	return indices;
}
