#ifndef MXCRITICALSECTION_H
#define MXCRITICALSECTION_H

#include <SDL3/SDL_mutex.h>

// SIZE 0x1c
class MxCriticalSection {
public:
	MxCriticalSection();
	~MxCriticalSection();

	static void SetDoMutex();

	void Enter();
	void Leave();

private:
	// [library:synchronization]
	// SDL uses the most efficient mutex implementation available on the target platform.
	// Originally this class allowed working with either a Win32 CriticalSection or Mutex,
	// but only CriticalSection was ever used and we don't need both anyway.
	SDL_Mutex* m_mutex;
};

#endif // MXCRITICALSECTION_H
