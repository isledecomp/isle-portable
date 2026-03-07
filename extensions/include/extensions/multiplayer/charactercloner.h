#pragma once

#include "legoactors.h"
#include "misc.h"

class LegoCharacterManager;
class LegoROI;

namespace Multiplayer
{

inline bool IsValidDisplayActorIndex(uint8_t p_index)
{
	return p_index < sizeOfArray(g_actorInfoInit);
}

class CharacterCloner {
public:
	// Creates an independent multi-part character ROI clone.
	// Same construction logic as CreateActorROI but with a unique name and
	// no side effects on g_actorInfo[].m_roi.
	static LegoROI* Clone(LegoCharacterManager* p_charMgr, const char* p_uniqueName, const char* p_characterType);
};

} // namespace Multiplayer
