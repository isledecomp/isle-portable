
#include "mxsemaphore.h"

#include "decomp.h"

DECOMP_SIZE_ASSERT(MxSemaphore, 0x08)

// FUNCTION: LEGO1 0x100c87d0
MxSemaphore::MxSemaphore()
{
	m_semaphore = NULL;
}

// FUNCTION: LEGO1 0x100c8800
MxResult MxSemaphore::Init(MxU32 p_initialCount, MxU32 p_maxCount)
{
	// [library:synchronization] No support for max count, but shouldn't be necessary
	MxResult result = FAILURE;

	if ((m_semaphore = SDL_CreateSemaphore(p_initialCount))) {
		result = SUCCESS;
	}

	return result;
}

// FUNCTION: LEGO1 0x100c8830
void MxSemaphore::Wait()
{
	// [library:synchronization] Removed timeout since only INFINITE is ever requested
	SDL_WaitSemaphore(m_semaphore);
}

// FUNCTION: LEGO1 0x100c8850
void MxSemaphore::Release()
{
	// [library:synchronization] Removed release count since only 1 is ever requested
	SDL_PostSemaphore(m_semaphore);
}
