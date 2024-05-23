#include "mxloopingsmkpresenter.h"

#include "mxautolock.h"
#include "mxdsmediaaction.h"
#include "mxdssubscriber.h"

DECOMP_SIZE_ASSERT(MxLoopingSmkPresenter, 0x724);

// FUNCTION: LEGO1 0x100b48b0
MxLoopingSmkPresenter::MxLoopingSmkPresenter()
{
	Init();
}

// FUNCTION: LEGO1 0x100b4950
MxLoopingSmkPresenter::~MxLoopingSmkPresenter()
{
	Destroy(TRUE);
}

// FUNCTION: LEGO1 0x100b49b0
void MxLoopingSmkPresenter::Init()
{
	m_elapsedDuration = 0;
	SetBit1(FALSE);
	SetBit2(FALSE);
}

// FUNCTION: LEGO1 0x100b49d0
void MxLoopingSmkPresenter::Destroy(MxBool p_fromDestructor)
{
	m_criticalSection.Enter();
	Init();
	m_criticalSection.Leave();

	if (!p_fromDestructor) {
		MxSmkPresenter::Destroy();
	}
}

// FUNCTION: LEGO1 0x100b4a00
void MxLoopingSmkPresenter::VTable0x88()
{
	// [library:libsmacker] Figure out if this functionality is still required
}

// FUNCTION: LEGO1 0x100b4a30
void MxLoopingSmkPresenter::NextFrame()
{
	MxStreamChunk* chunk = NextChunk();

	if (chunk->GetChunkFlags() & DS_CHUNK_END_OF_STREAM) {
		ProgressTickleState(e_repeating);
	}
	else {
		LoadFrame(chunk);
		LoopChunk(chunk);
		m_elapsedDuration += 1000 / ((MxDSMediaAction*) m_action)->GetFramesPerSecond();
	}

	m_subscriber->FreeDataChunk(chunk);
}

// FUNCTION: LEGO1 0x100b4a90
void MxLoopingSmkPresenter::VTable0x8c()
{
	if (m_action->GetDuration() < m_elapsedDuration) {
		ProgressTickleState(e_freezing);
	}
	else {
		MxStreamChunk* chunk;
		m_loopingChunkCursor->Current(chunk);
		LoadFrame(chunk);
		m_elapsedDuration += 1000 / ((MxDSMediaAction*) m_action)->GetFramesPerSecond();
	}
}

// FUNCTION: LEGO1 0x100b4b00
void MxLoopingSmkPresenter::RepeatingTickle()
{
	for (MxS16 i = 0; i < m_unk0x5c; i++) {
		if (!m_loopingChunkCursor->HasMatch()) {
			MxStreamChunk* chunk;
			MxStreamChunkListCursor cursor(m_loopingChunks);

			cursor.Last(chunk);
			MxLong time = chunk->GetTime();

			cursor.First(chunk);

			time -= chunk->GetTime();
			time += 1000 / ((MxDSMediaAction*) m_action)->GetFramesPerSecond();

			cursor.Reset();
			while (cursor.Next(chunk)) {
				chunk->SetTime(chunk->GetTime() + time);
			}

			m_loopingChunkCursor->Next();
		}

		MxStreamChunk* chunk;
		m_loopingChunkCursor->Current(chunk);

		if (m_action->GetElapsedTime() < chunk->GetTime()) {
			break;
		}

		VTable0x8c();

		m_loopingChunkCursor->Next(chunk);

		if (m_currentTickleState != e_repeating) {
			break;
		}
	}
}

// FUNCTION: LEGO1 0x100b4cd0
MxResult MxLoopingSmkPresenter::AddToManager()
{
	AUTOLOCK(m_criticalSection);
	return MxSmkPresenter::AddToManager();
}

// FUNCTION: LEGO1 0x100b4d40
void MxLoopingSmkPresenter::Destroy()
{
	Destroy(FALSE);
}
