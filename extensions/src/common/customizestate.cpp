#include "extensions/common/customizestate.h"

#include "legoactors.h"
#include "misc.h"

#include <SDL3/SDL_stdinc.h>

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

	// Derive dependent parts (must match Unpack derivation rules)
	colorIndices[c_bodyPart] = colorIndices[c_infogronPart];
	colorIndices[c_headPart] = colorIndices[c_infohatPart];
	colorIndices[c_clawlftPart] = colorIndices[c_armlftPart];
	colorIndices[c_clawrtPart] = colorIndices[c_armrtPart];

	hatVariantIndex = info.m_parts[c_infohatPart].m_partNameIndex;
	sound = (uint8_t) info.m_sound;
	move = (uint8_t) info.m_move;
	mood = info.m_mood;
}

void CustomizeState::Pack(uint8_t p_out[5]) const
{
	// byte 0: hatVariantIndex(5 bits) | reserved(3 bits)
	p_out[0] = (hatVariantIndex & 0x1F);

	// byte 1: sound(4 bits) | move(2 bits) | mood(2 bits)
	p_out[1] = (sound & 0x0F) | ((move & 0x03) << 4) | ((mood & 0x03) << 6);

	// byte 2: infohatColor(4 bits) | infogronColor(4 bits)
	p_out[2] = (colorIndices[c_infohatPart] & 0x0F) | ((colorIndices[c_infogronPart] & 0x0F) << 4);

	// byte 3: armlftColor(4 bits) | armrtColor(4 bits)
	p_out[3] = (colorIndices[c_armlftPart] & 0x0F) | ((colorIndices[c_armrtPart] & 0x0F) << 4);

	// byte 4: leglftColor(4 bits) | legrtColor(4 bits)
	p_out[4] = (colorIndices[c_leglftPart] & 0x0F) | ((colorIndices[c_legrtPart] & 0x0F) << 4);
}

void CustomizeState::Unpack(const uint8_t p_in[5])
{
	// byte 0: hatVariantIndex(5 bits) | reserved(3 bits)
	hatVariantIndex = p_in[0] & 0x1F;

	// byte 1: sound(4 bits) | move(2 bits) | mood(2 bits)
	sound = p_in[1] & 0x0F;
	move = (p_in[1] >> 4) & 0x03;
	mood = (p_in[1] >> 6) & 0x03;

	// byte 2: infohatColor(4 bits) | infogronColor(4 bits)
	colorIndices[c_infohatPart] = p_in[2] & 0x0F;
	colorIndices[c_infogronPart] = (p_in[2] >> 4) & 0x0F;

	// byte 3: armlftColor(4 bits) | armrtColor(4 bits)
	colorIndices[c_armlftPart] = p_in[3] & 0x0F;
	colorIndices[c_armrtPart] = (p_in[3] >> 4) & 0x0F;

	// byte 4: leglftColor(4 bits) | legrtColor(4 bits)
	colorIndices[c_leglftPart] = p_in[4] & 0x0F;
	colorIndices[c_legrtPart] = (p_in[4] >> 4) & 0x0F;

	// Derive non-independent parts
	colorIndices[c_bodyPart] = colorIndices[c_infogronPart];
	colorIndices[c_headPart] = colorIndices[c_infohatPart];
	colorIndices[c_clawlftPart] = colorIndices[c_armlftPart];
	colorIndices[c_clawrtPart] = colorIndices[c_armrtPart];
}

bool CustomizeState::operator==(const CustomizeState& p_other) const
{
	return SDL_memcmp(this, &p_other, sizeof(CustomizeState)) == 0;
}
