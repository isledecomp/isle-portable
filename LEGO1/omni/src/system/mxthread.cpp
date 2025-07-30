#include "mxthread.h"

#include "decomp.h"

#include <SDL3/SDL_timer.h>
#ifdef PS2
#include <kernel.h>
#endif

DECOMP_SIZE_ASSERT(MxThread, 0x1c)

// FUNCTION: LEGO1 0x100bf510
// FUNCTION: BETA10 0x10147540
MxThread::MxThread()
{
#ifdef PS2
	m_thread = 0;
#else
	m_thread = NULL;
#endif
	m_running = TRUE;
}

// FUNCTION: LEGO1 0x100bf5a0
// FUNCTION: BETA10 0x101475d0
MxThread::~MxThread()
{
	if (m_thread) {
#ifdef PS2
		ReleaseWaitThread(m_thread);
		DeleteThread(m_thread);
		m_thread = 0;
#else
		SDL_WaitThread(m_thread, NULL);
#endif
	}
}

// FUNCTION: LEGO1 0x100bf610
// FUNCTION: BETA10 0x10147655
MxResult MxThread::Start(MxS32 p_stackSize, MxS32 p_flag)
{
	MxResult result = FAILURE;

	if (m_semaphore.Init(0, 1) != SUCCESS) {
#ifdef PS2
		ee_thread_t thread;
		thread.func = (void*)&(MxThread::ThreadProc);
		thread .stack_size = p_stackSize * 4;
		thread.initial_priority = 0x18;
		int thid = CreateThread(&thread);

		if (thid >= 0) {
			MxThread* self = this;
			int start = StartThread(thid, &self);
			if (start >= 0) {
				result = SUCCESS;
				m_thread = thid; // store thread ID if needed
			}
		}
	}
#else
		goto done;
	}

	{
		const SDL_PropertiesID props = SDL_CreateProperties();
		SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_ENTRY_FUNCTION_POINTER, (void*) MxThread::ThreadProc);
		SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_USERDATA_POINTER, this);
		SDL_SetNumberProperty(props, SDL_PROP_THREAD_CREATE_STACKSIZE_NUMBER, p_stackSize * 4);

		if (!(m_thread = SDL_CreateThreadWithProperties(props))) {
			goto done;
		}

		SDL_DestroyProperties(props);
	}
#endif

	result = SUCCESS;

done:
	return result;
}

// FUNCTION: LEGO1 0x100bf660
// FUNCTION: BETA10 0x101476ee
void MxThread::Sleep(MxS32 p_milliseconds)
{
	SDL_Delay(p_milliseconds);
}

// FUNCTION: BETA10 0x10147710
void MxThread::ResumeThread()
{
	// unused
}

// FUNCTION: BETA10 0x10147733
void MxThread::SuspendThread()
{
	// unused
}

// FUNCTION: BETA10 0x10147756
bool MxThread::TerminateThread(MxU32 p_exitCode)
{
	// unused
	return false;
}

// FUNCTION: BETA10 0x10147793
MxS32 MxThread::GetThreadPriority(MxU16& p_priority)
{
	// unused
	return -1;
}

// FUNCTION: BETA10 0x101477c8
bool MxThread::SetThreadPriority(MxU16 p_priority)
{
	// unused
	return false;
}

// FUNCTION: LEGO1 0x100bf670
// FUNCTION: BETA10 0x1014780a
void MxThread::Terminate()
{
	m_running = FALSE;
	m_semaphore.Acquire();
}

// FUNCTION: LEGO1 0x100bf680
// FUNCTION: BETA10 0x1014783b
int MxThread::ThreadProc(void* p_thread)
{
	return static_cast<MxThread*>(p_thread)->Run();
}

// FUNCTION: LEGO1 0x100bf690
// FUNCTION: BETA10 0x10147855
MxResult MxThread::Run()
{
	m_semaphore.Release();
	return SUCCESS;
}
