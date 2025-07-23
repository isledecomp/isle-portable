
#include "mxsemaphore.h"

#include "decomp.h"

DECOMP_SIZE_ASSERT(MxSemaphore, 0x08)

// FUNCTION: LEGO1 0x100c87d0
// FUNCTION: BETA10 0x10159260
MxSemaphore::MxSemaphore()
{
	m_semaphore = NULL;
}

// FUNCTION: LEGO1 0x100c8800
// FUNCTION: BETA10 0x101592d5
MxResult MxSemaphore::Init(MxU32 p_initialCount, MxU32 p_maxCount)
{
	// [library:synchronization] No support for max count, but shouldn't be necessary
	MxResult result = FAILURE;

	m_semaphore = SDL_CreateSemaphore(p_initialCount);
	if (!m_semaphore) {
		goto done;
	}

	result = SUCCESS;

done:
	return result;
}

// FUNCTION: LEGO1 0x100c8830
// FUNCTION: BETA10 0x10159332
void MxSemaphore::Acquire()
{
	// [library:synchronization] Removed timeout since only INFINITE is ever requested
	SDL_WaitSemaphore(m_semaphore);
}

// FUNCTION: BETA10 0x10159385
void MxSemaphore::TryAcquire()
{
	// unused
}

// FUNCTION: LEGO1 0x100c8850
// FUNCTION: BETA10 0x101593aa
void MxSemaphore::Release()
{
	// [library:synchronization] Removed release count since only 1 is ever requested
	SDL_SignalSemaphore(m_semaphore);
}
