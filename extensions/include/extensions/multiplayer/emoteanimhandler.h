#pragma once

#include "extensions/common/characteranimator.h"

#include <cstdint>

class LegoAnim;
class LegoROI;

namespace Extensions
{
namespace Common
{
struct PropGroup;
}
} // namespace Extensions

namespace Multiplayer
{

// Per-phase emote data: animation name and optional sound effect.
struct EmotePhase {
	const char* anim;  // Animation name (nullptr = unused phase)
	const char* sound; // Sound key for LegoCacheSoundManager (nullptr = silent)
};

// Emote table entry: two phases (phase 1 = primary, phase 2 = recovery for multi-part emotes).
struct EmoteEntry {
	EmotePhase phases[2];
};

extern const EmoteEntry g_emoteEntries[];
extern const int g_emoteAnimCount;

// Returns true if the emote is a multi-part stateful emote (has a phase-2 animation).
inline bool IsMultiPartEmote(uint8_t p_emoteId)
{
	return p_emoteId < g_emoteAnimCount && g_emoteEntries[p_emoteId].phases[1].anim != nullptr;
}

// IExtraAnimHandler implementation for emote animations.
// Delegates to the emote table for animation names, sound keys, and multi-part detection.
class EmoteAnimHandler : public Extensions::Common::IExtraAnimHandler {
public:
	EmoteAnimHandler() = default;

	bool IsValid(uint8_t p_id) const override;
	bool IsMultiPart(uint8_t p_id) const override;
	const char* GetAnimName(uint8_t p_id, int p_phase) const override;
	const char* GetSoundName(uint8_t p_id, int p_phase) const override;
	void BuildProps(
		Extensions::Common::PropGroup& p_group,
		LegoAnim* p_anim,
		LegoROI* p_playerROI,
		uint32_t p_propSuffix
	) override;
};

} // namespace Multiplayer
