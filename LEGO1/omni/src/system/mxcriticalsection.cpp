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

#ifdef BETA10
// FUNCTION: BETA10 0x1013c725
void MxCriticalSection::Enter(unsigned long p_threadId, const char* filename, int line)
{
	DWORD result;
	FILE* file;

	if (m_mutex != NULL) {
		result = WaitForSingleObject(m_mutex, 5000);
		if (result == WAIT_FAILED) {
			file = fopen("C:\\DEADLOCK.TXT", "a");
			if (file != NULL) {
				fprintf(file, "mutex timeout occurred!\n");
				fprintf(file, "file: %s, line: %d\n", filename, line);
				fclose(file);
			}

			abort();
		}
	}
	else {
		EnterCriticalSection(&m_criticalSection);
	}

	// There is way more structure in here, and the MxCriticalSection class is much larger in BETA10.
	// The LEGO1 compilation is very unlikely to profit from a further decompilation here.
}
#else
// FUNCTION: LEGO1 0x100b6d80
void MxCriticalSection::Enter()
{
	SDL_LockMutex(m_mutex);
}
#endif

// FUNCTION: LEGO1 0x100b6de0
// FUNCTION: BETA10 0x1013c7ef
void MxCriticalSection::Leave()
{
	SDL_UnlockMutex(m_mutex);
}

// FUNCTION: LEGO1 0x100b6e00
void MxCriticalSection::SetDoMutex()
{
	g_useMutex = TRUE;
}
