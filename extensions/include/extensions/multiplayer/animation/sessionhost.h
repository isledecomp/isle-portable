#pragma once

#include <cstdint>
#include <map>
#include <vector>

namespace Multiplayer::Animation
{

class Catalog;
struct CatalogEntry;
enum class CoordinationState : uint8_t;

struct SessionSlot {
	uint32_t peerId;  // 0 = unfilled
	int8_t charIndex; // g_characters index, or -1 for spectator

	bool IsSpectator() const { return charIndex < 0; }
};

struct AnimSession {
	uint16_t animIndex;
	CoordinationState state;
	std::vector<SessionSlot> slots;
	uint32_t countdownEndTime; // SDL_GetTicks timestamp when countdown expires
};

class SessionHost {
public:
	void SetCatalog(const Catalog* p_catalog);

	bool HandleInterest(
		uint32_t p_peerId,
		uint16_t p_animIndex,
		uint8_t p_displayActorIndex,
		std::vector<uint16_t>& p_changedAnims
	);
	bool HandleCancel(uint32_t p_peerId, std::vector<uint16_t>& p_changedAnims);
	bool HandlePlayerRemoved(uint32_t p_peerId, std::vector<uint16_t>& p_changedAnims);

	// Returns animIndices of all sessions ready to play
	std::vector<uint16_t> Tick(uint32_t p_now);

	void StartCountdown(uint16_t p_animIndex);
	void RevertCountdown(uint16_t p_animIndex);

	void Reset();
	void EraseSession(uint16_t p_animIndex);

	const AnimSession* FindSession(uint16_t p_animIndex) const;
	const std::map<uint16_t, AnimSession>& GetSessions() const;
	bool AreAllSlotsFilled(uint16_t p_animIndex) const;

	static uint16_t ComputeCountdownMs(const AnimSession& p_session, uint32_t p_now);

	// Reconstruct slot charIndex assignments from CatalogEntry::performerMask.
	// Same iteration order as CreateSession — deterministic across all clients.
	static std::vector<int8_t> ComputeSlotCharIndices(const CatalogEntry* p_entry);

	bool HasCountdownSession() const;

private:
	AnimSession CreateSession(const CatalogEntry* p_entry, uint16_t p_animIndex);
	bool TryAssignSlot(AnimSession& p_session, uint32_t p_peerId, int8_t p_charIndex);
	bool AllSlotsFilled(const AnimSession& p_session) const;
	void RemovePlayerFromAllSessions(uint32_t p_peerId, std::vector<uint16_t>& p_changedAnims);
	void RemovePlayerFromSessions(
		uint32_t p_peerId,
		bool p_includePlayingSessions,
		std::vector<uint16_t>& p_changedAnims
	);

	const Catalog* m_catalog = nullptr;
	std::map<uint16_t, AnimSession> m_sessions;

	static const uint32_t COUNTDOWN_DURATION_MS = 4000;
};

} // namespace Multiplayer::Animation
