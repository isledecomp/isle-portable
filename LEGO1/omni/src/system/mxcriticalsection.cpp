#include "mxcriticalsection.h"

#include "decomp.h"
#include "mxtypes.h"

DECOMP_SIZE_ASSERT(MxCriticalSection, 0x1c)

// GLOBAL: LEGO1 0x10101e78
MxS32 g_useMutex = FALSE;

// FUNCTION: LEGO1 0x100b6d20
MxCriticalSection::MxCriticalSection()
{
	m_mutex = SDL_CreateMutex();
}

// FUNCTION: LEGO1 0x100b6d60
MxCriticalSection::~MxCriticalSection()
{
	if (m_mutex != NULL) {
		SDL_DestroyMutex(m_mutex);
	}
}

// FUNCTION: LEGO1 0x100b6d80
// FUNCTION: BETA10 0x1013c725
void MxCriticalSection::Enter()
{
	SDL_LockMutex(m_mutex);
}

// FUNCTION: LEGO1 0x100b6de0
void MxCriticalSection::Leave()
{
	SDL_UnlockMutex(m_mutex);
}

// FUNCTION: LEGO1 0x100b6e00
void MxCriticalSection::SetDoMutex()
{
	g_useMutex = TRUE;
}
