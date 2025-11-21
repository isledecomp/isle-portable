#include "mxthread.h"

#include "decomp.h"

#include <mortar/mortar_timer.h>

DECOMP_SIZE_ASSERT(MxThread, 0x1c)

// FUNCTION: LEGO1 0x100bf510
// FUNCTION: BETA10 0x10147540
MxThread::MxThread()
{
	m_thread = NULL;
	m_running = TRUE;
}

// FUNCTION: LEGO1 0x100bf5a0
// FUNCTION: BETA10 0x101475d0
MxThread::~MxThread()
{
	if (m_thread) {
		MORTAR_WaitThread(m_thread, NULL);
	}
}

// FUNCTION: LEGO1 0x100bf610
// FUNCTION: BETA10 0x10147655
MxResult MxThread::Start(MxS32 p_stackSize, MxS32 p_flag)
{
	MxResult result = FAILURE;

	if (m_semaphore.Init(0, 1) != SUCCESS) {
		goto done;
	}

	if (!(m_thread = MORTAR_CreateThread(MxThread::ThreadProc, this, p_stackSize * 4))) {
		goto done;
	}

	result = SUCCESS;

done:
	return result;
}

// FUNCTION: LEGO1 0x100bf660
// FUNCTION: BETA10 0x101476ee
void MxThread::Sleep(MxS32 p_milliseconds)
{
	MORTAR_Delay(p_milliseconds);
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
