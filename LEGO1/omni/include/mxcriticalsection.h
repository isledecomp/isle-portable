#ifndef MXCRITICALSECTION_H
#define MXCRITICALSECTION_H

#include <SDL3/SDL_mutex.h>

// SIZE 0x1c
class MxCriticalSection {
public:
	MxCriticalSection();
	~MxCriticalSection();

	static void SetDoMutex();

#ifdef BETA10
	void Enter(unsigned long p_threadId, const char* filename, int line);
#else
	void Enter();
#endif
	void Leave();

private:
	// [library:synchronization]
	// SDL uses the most efficient mutex implementation available on the target platform.
	// Originally this class allowed working with either a Win32 CriticalSection or Mutex,
	// but only CriticalSection was ever used and we don't need both anyway.
	SDL_Mutex* m_mutex;
};

#ifdef BETA10
// TODO: Not quite correct yet, the second argument becomes a relocated value
#define ENTER(criticalSection) criticalSection.Enter(-1, NULL, 0)
#else
#define ENTER(criticalSection) criticalSection.Enter()
#endif

#endif // MXCRITICALSECTION_H
