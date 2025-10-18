#include "legostate.h"

#include <random>
#include <stdlib.h>

DECOMP_SIZE_ASSERT(LegoState, 0x08)
DECOMP_SIZE_ASSERT(LegoState::Playlist, 0x0c)

// FUNCTION: LEGO1 0x10014d00
// FUNCTION: BETA10 0x10022580
MxU32 LegoState::Playlist::Next()
{
	MxU32 objectId;

	switch (m_mode) {
	case e_loop:
		objectId = m_objectIds[m_nextIndex];
		if (m_nextIndex - m_length == -1) {
			m_nextIndex = 0;
		}
		else {
			m_nextIndex++;
		}
		break;

	case e_once:
		objectId = m_objectIds[m_nextIndex];
		if (m_length > m_nextIndex + 1) {
			m_nextIndex++;
		}
		break;

	case e_random: {
#ifdef __WIIU__
		static std::mt19937 rng{std::random_device{}()};
		std::uniform_int_distribution<MxS32> dist(0, m_length - 1);
		m_nextIndex = dist(rng);
#else
		m_nextIndex = SDL_rand(m_length);
#endif
		objectId = m_objectIds[m_nextIndex];
		break;
	}

	case e_loopSkipFirst:
		objectId = m_objectIds[m_nextIndex];
		if (m_nextIndex - m_length == -1) {
			m_nextIndex = 1;
		}
		else {
			m_nextIndex++;
		}
	}

	return objectId;
}

// FUNCTION: LEGO1 0x10014de0
MxBool LegoState::Playlist::Contains(MxU32 p_objectId)
{
	for (MxS16 i = 0; i < m_length; i++) {
		if (m_objectIds[i] == p_objectId) {
			return TRUE;
		}
	}

	return FALSE;
}
