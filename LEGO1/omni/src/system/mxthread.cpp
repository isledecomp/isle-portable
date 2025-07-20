#include "mxthread.h"

#include "decomp.h"

#include <SDL3/SDL_timer.h>
#ifdef PSP
#include <pspkernel.h>
#endif

DECOMP_SIZE_ASSERT(MxThread, 0x1c)

// FUNCTION: LEGO1 0x100bf510
MxThread::MxThread()
{
#ifdef PSP
	m_thread = 0;
#else
	m_thread = NULL;
#endif
	m_running = TRUE;
}

// FUNCTION: LEGO1 0x100bf5a0
MxThread::~MxThread()
{
	if (m_thread) {
#ifdef PSP
		sceKernelWaitThreadEnd(m_thread, NULL);
		sceKernelDeleteThread(m_thread);
		m_thread = 0;
#else
		SDL_WaitThread(m_thread, NULL);
#endif
	}
}

// FUNCTION: LEGO1 0x100bf610
MxResult MxThread::Start(MxS32 p_stackSize, MxS32 p_flag)
{
	MxResult result = FAILURE;

	if (m_semaphore.Init(0, 1) == SUCCESS) {
#ifdef PSP
		int thid = sceKernelCreateThread(
			"MxThread",
			ThreadProc,
			0x18, // priority (0x18 is typical)
			p_stackSize * 4,
			0,
			NULL
		);

		if (thid >= 0) {
			MxThread* self = this;
			int start = sceKernelStartThread(thid, sizeof(MxThread*), &self);
			if (start >= 0) {
				result = SUCCESS;
				m_thread = thid; // store thread ID if needed
			}
		}
#else
		const SDL_PropertiesID props = SDL_CreateProperties();
		SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_ENTRY_FUNCTION_POINTER, (void*) MxThread::ThreadProc);
		SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_USERDATA_POINTER, this);
		SDL_SetNumberProperty(props, SDL_PROP_THREAD_CREATE_STACKSIZE_NUMBER, p_stackSize * 4);

		if ((m_thread = SDL_CreateThreadWithProperties(props))) {
			result = SUCCESS;
		}

		SDL_DestroyProperties(props);
#endif
	}

	return result;
}

// FUNCTION: LEGO1 0x100bf660
void MxThread::Sleep(MxS32 p_milliseconds)
{
	SDL_Delay(p_milliseconds);
}

// FUNCTION: LEGO1 0x100bf670
void MxThread::Terminate()
{
	m_running = FALSE;
	m_semaphore.Wait();
}

#ifdef PSP
int MxThread::ThreadProc(SceSize args, void* argp)
{
	MxThread* self = *(MxThread**) argp;
	return self->Run();
}
#else
// FUNCTION: LEGO1 0x100bf680
int MxThread::ThreadProc(void* p_thread)
{
	return static_cast<MxThread*>(p_thread)->Run();
}
#endif

// FUNCTION: LEGO1 0x100bf690
MxResult MxThread::Run()
{
	m_semaphore.Release();
	return SUCCESS;
}
