#pragma once

class LegoCharacterManager;
class LegoROI;

namespace Multiplayer
{

class CharacterCloner {
public:
	// Creates an independent multi-part character ROI clone.
	// Same construction logic as CreateActorROI but with a unique name and
	// no side effects on g_actorInfo[].m_roi.
	static LegoROI* Clone(LegoCharacterManager* p_charMgr, const char* p_uniqueName, const char* p_characterType);
};

} // namespace Multiplayer
