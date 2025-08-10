#include "legosoundmanager.h"

#include "legocachesoundmanager.h"
#include "mxautolock.h"
#include "mxmain.h"

#include <assert.h>

DECOMP_SIZE_ASSERT(LegoSoundManager, 0x44)

// FUNCTION: LEGO1 0x100298a0
// FUNCTION: BETA10 0x100cffb0
LegoSoundManager::LegoSoundManager()
{
	Init();
}

// FUNCTION: LEGO1 0x10029940
// FUNCTION: BETA10 0x100d0027
LegoSoundManager::~LegoSoundManager()
{
	Destroy(TRUE);
}

// FUNCTION: LEGO1 0x100299a0
// FUNCTION: BETA10 0x100d0099
void LegoSoundManager::Init()
{
	m_cacheSoundManager = NULL;
}

// FUNCTION: LEGO1 0x100299b0
// FUNCTION: BETA10 0x100d00c9
void LegoSoundManager::Destroy(MxBool p_fromDestructor)
{
	delete m_cacheSoundManager;
	Init();

	if (!p_fromDestructor) {
		MxSoundManager::Destroy();
	}
}

// FUNCTION: LEGO1 0x100299f0
// FUNCTION: BETA10 0x100d0129
MxResult LegoSoundManager::Create(MxU32 p_frequencyMS, MxBool p_createThread)
{
	MxResult result = FAILURE;
	MxBool locked = FALSE;

	if (MxSoundManager::Create(10, FALSE) != SUCCESS) {
		goto done;
	}

	ENTER(m_criticalSection);
	locked = TRUE;
	m_cacheSoundManager = new LegoCacheSoundManager;
	assert(m_cacheSoundManager);
	result = SUCCESS;

done:
	if (result != SUCCESS) {
		Destroy();
	}

	if (locked) {
		m_criticalSection.Leave();
	}

	return result;
}

// FUNCTION: LEGO1 0x1002a390
// FUNCTION: BETA10 0x100d02ed
void LegoSoundManager::Destroy()
{
	Destroy(FALSE);
}

// FUNCTION: LEGO1 0x1002a3a0
// FUNCTION: BETA10 0x100d030d
MxResult LegoSoundManager::Tickle()
{
	MxSoundManager::Tickle();

	AUTOLOCK(m_criticalSection);
	return m_cacheSoundManager->Tickle();
}

// FUNCTION: LEGO1 0x1002a410
// FUNCTION: BETA10 0x100d03a5
void LegoSoundManager::UpdateListener(
	const float* p_position,
	const float* p_direction,
	const float* p_up,
	const float* p_velocity
)
{
	if (MxOmni::IsSound3D()) {
		// [library:audio]
		// miniaudio expects the right-handed OpenGL coordinate system, while LEGO Island
		// uses DirectX' left-handed system. The Z-axis needs to be inverted.

		if (p_position != NULL) {
			ma_engine_listener_set_position(m_engine, 0, p_position[0], p_position[1], -p_position[2]);
		}

		if (p_direction != NULL && p_up != NULL) {
			ma_engine_listener_set_direction(m_engine, 0, p_direction[0], p_direction[1], -p_direction[2]);
			ma_engine_listener_set_world_up(m_engine, 0, p_up[0], p_up[1], -p_up[2]);
		}

		if (p_velocity != NULL) {
			ma_engine_listener_set_velocity(m_engine, 0, p_velocity[0], p_velocity[1], -p_velocity[2]);
		}
	}
}
