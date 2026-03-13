#pragma once

#include "extensions/common/constants.h"
#include "legoactors.h"
#include "misc.h"

#include <SDL3/SDL_stdinc.h>
#include <cstdint>

class LegoCharacterManager;
class LegoROI;

namespace Extensions
{
namespace Common
{

inline bool IsValidDisplayActorIndex(uint8_t p_index)
{
	return p_index < sizeOfArray(g_actorInfoInit);
}

inline uint8_t ResolveDisplayActorIndex(const char* p_name)
{
	for (int i = 0; i < static_cast<int>(sizeOfArray(g_actorInfoInit)); i++) {
		if (!SDL_strcasecmp(g_actorInfoInit[i].m_name, p_name)) {
			return static_cast<uint8_t>(i);
		}
	}
	return DISPLAY_ACTOR_NONE;
}

class CharacterCloner {
public:
	// Creates an independent multi-part character ROI clone.
	// Same construction logic as CreateActorROI but with a unique name and
	// no side effects on g_actorInfo[].m_roi.
	static LegoROI* Clone(LegoCharacterManager* p_charMgr, const char* p_uniqueName, const char* p_characterType);
};

} // namespace Common
} // namespace Extensions
