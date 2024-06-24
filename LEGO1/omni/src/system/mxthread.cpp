#include "mxthread.h"

#include "decomp.h"

#include <SDL3/SDL_timer.h>

DECOMP_SIZE_ASSERT(MxThread, 0x1c)

// FUNCTION: LEGO1 0x100bf510
MxThread::MxThread()
{
	m_thread = NULL;
	m_running = TRUE;
}

// FUNCTION: LEGO1 0x100bf5a0
MxThread::~MxThread()
{
	if (m_thread) {
		SDL_WaitThread(m_thread, NULL);
	}
}

// FUNCTION: LEGO1 0x100bf610
MxResult MxThread::Start(MxS32 p_stack, MxS32 p_flag)
{
	MxResult result = FAILURE;

	if (m_semaphore.Init(0, 1) == SUCCESS) {
		const SDL_PropertiesID props = SDL_CreateProperties();
		SDL_SetProperty(props, SDL_PROP_THREAD_CREATE_ENTRY_FUNCTION_POINTER, (void*)MxThread::ThreadProc);
		SDL_SetProperty(props, SDL_PROP_THREAD_CREATE_USERDATA_POINTER, this);
		SDL_SetNumberProperty(props, SDL_PROP_THREAD_CREATE_STACKSIZE_NUMBER, p_stack << 2);

		if ((m_thread = SDL_CreateThreadWithProperties(props))) {
			result = SUCCESS;
		}

		SDL_DestroyProperties(props);
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

// FUNCTION: LEGO1 0x100bf680
int MxThread::ThreadProc(void* p_thread)
{
	return static_cast<MxThread*>(p_thread)->Run();
}

// FUNCTION: LEGO1 0x100bf690
MxResult MxThread::Run()
{
	m_semaphore.Release();
	return SUCCESS;
}
