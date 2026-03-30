#include "extensions/common/customizestate.h"

#include "legoactors.h"
#include "misc.h"

using namespace Extensions::Common;

void CustomizeState::InitFromActorInfo(uint8_t p_actorInfoIndex)
{
	if (p_actorInfoIndex >= sizeOfArray(g_actorInfoInit)) {
		return;
	}

	const LegoActorInfo& info = g_actorInfoInit[p_actorInfoIndex];

	// Set the 6 independent colorable parts from actor info
	colorIndices[c_infohatPart] = info.m_parts[c_infohatPart].m_nameIndex;
	colorIndices[c_infogronPart] = info.m_parts[c_infogronPart].m_nameIndex;
	colorIndices[c_armlftPart] = info.m_parts[c_armlftPart].m_nameIndex;
	colorIndices[c_armrtPart] = info.m_parts[c_armrtPart].m_nameIndex;
	colorIndices[c_leglftPart] = info.m_parts[c_leglftPart].m_nameIndex;
	colorIndices[c_legrtPart] = info.m_parts[c_legrtPart].m_nameIndex;

	DeriveDependentIndices();

	hatVariantIndex = info.m_parts[c_infohatPart].m_partNameIndex;
	sound = (uint8_t) info.m_sound;
	move = (uint8_t) info.m_move;
	mood = info.m_mood;
}

void CustomizeState::DeriveDependentIndices()
{
	colorIndices[c_bodyPart] = colorIndices[c_infogronPart];
	colorIndices[c_headPart] = colorIndices[c_infohatPart];
	colorIndices[c_clawlftPart] = colorIndices[c_armlftPart];
	colorIndices[c_clawrtPart] = colorIndices[c_armrtPart];
}
