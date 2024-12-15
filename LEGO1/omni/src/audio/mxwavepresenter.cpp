#include "mxwavepresenter.h"

#include "decomp.h"
#include "define.h"
#include "mxautolock.h"
#include "mxdssound.h"
#include "mxdssubscriber.h"
#include "mxmisc.h"
#include "mxomni.h"
#include "mxsoundmanager.h"
#include "mxutilities.h"

#include <assert.h>

DECOMP_SIZE_ASSERT(MxWavePresenter, 0x6c);
DECOMP_SIZE_ASSERT(MxWavePresenter::WaveFormat, 0x18);

// FUNCTION: LEGO1 0x100b1ad0
void MxWavePresenter::Init()
{
	m_waveFormat = NULL;
	SDL_zero(m_rb);
	SDL_zero(m_sound);
	m_chunkLength = 0;
	m_started = FALSE;
	m_is3d = FALSE;
	m_paused = FALSE;
}

// FUNCTION: LEGO1 0x100b1af0
MxResult MxWavePresenter::AddToManager()
{
	MxResult result = MxSoundPresenter::AddToManager();
	Init();
	return result;
}

// FUNCTION: LEGO1 0x100b1b10
void MxWavePresenter::Destroy(MxBool p_fromDestructor)
{
	ma_sound_uninit(&m_sound);
	ma_pcm_rb_uninit(&m_rb);

	if (m_waveFormat) {
		delete[] ((MxU8*) m_waveFormat);
	}

	Init();

	if (!p_fromDestructor) {
		MxSoundPresenter::Destroy(FALSE);
	}
}

// FUNCTION: LEGO1 0x100b1bd0
MxBool MxWavePresenter::WriteToSoundBuffer(void* p_audioPtr, MxU32 p_length)
{
	ma_uint32 requestedFrames =
		ma_calculate_buffer_size_in_frames_from_milliseconds(g_millisecondsPerChunk, m_waveFormat->m_samplesPerSec);
	ma_uint32 acquiredFrames = requestedFrames;
	void* bufferOut;

	ma_pcm_rb_acquire_write(&m_rb, &acquiredFrames, &bufferOut);

	// [library:audio] If there isn't enough space in the buffer for a full chunk, try again later.
	if (acquiredFrames != requestedFrames) {
		ma_pcm_rb_commit_write(&m_rb, 0);
		return FALSE;
	}

	ma_uint32 acquiredBytes = acquiredFrames * ma_get_bytes_per_frame(m_rb.format, m_rb.channels);
	assert(p_length <= acquiredBytes);

	memcpy(bufferOut, p_audioPtr, p_length);

	// [library:audio] Pad with silence data if we don't have a full chunk.
	if (p_length < acquiredBytes) {
		memset((ma_uint8*) bufferOut + p_length, m_silenceData, acquiredBytes - p_length);
	}

	ma_pcm_rb_commit_write(&m_rb, acquiredFrames);
	return TRUE;
}

// FUNCTION: LEGO1 0x100b1cf0
void MxWavePresenter::ReadyTickle()
{
	MxStreamChunk* chunk = NextChunk();

	if (chunk) {
		m_waveFormat = (WaveFormat*) new MxU8[chunk->GetLength()];
		memcpy(m_waveFormat, chunk->GetData(), chunk->GetLength());
		m_subscriber->FreeDataChunk(chunk);
		ParseExtra();
		ProgressTickleState(e_starting);
	}
}

// FUNCTION: LEGO1 0x100b1d50
void MxWavePresenter::StartingTickle()
{
	MxStreamChunk* chunk = CurrentChunk();

	if (chunk && m_action->GetElapsedTime() >= chunk->GetTime()) {
		MxBool success = FALSE;
		m_chunkLength = chunk->GetLength();

		assert(m_waveFormat->m_formatTag == g_supportedFormatTag);
		assert(m_waveFormat->m_bitsPerSample == 8 || m_waveFormat->m_bitsPerSample == 16);

		if (m_waveFormat->m_bitsPerSample == 8) {
			m_silenceData = 0x7F;
		}

		if (m_waveFormat->m_bitsPerSample == 16) {
			m_silenceData = 0;
		}

		// [library:audio]
		// If we have a repeating action (MxDSAction::c_looping set), we must make sure the ring buffer
		// is large enough to contain the entire sound at once. The size must be a multiple of `g_millisecondsPerChunk`
		if (ma_pcm_rb_init(
				m_waveFormat->m_bitsPerSample == 16 ? ma_format_s16 : ma_format_u8,
				m_waveFormat->m_channels,
				ma_calculate_buffer_size_in_frames_from_milliseconds(
					m_action->GetFlags() & MxDSAction::c_looping
						? (m_action->GetDuration() / m_action->GetLoopCount() + g_millisecondsPerChunk - 1) /
							  g_millisecondsPerChunk * g_millisecondsPerChunk
						: g_rbSizeInMilliseconds,
					m_waveFormat->m_samplesPerSec
				),
				NULL,
				NULL,
				&m_rb
			) != MA_SUCCESS) {
			goto done;
		}

		ma_pcm_rb_set_sample_rate(&m_rb, m_waveFormat->m_samplesPerSec);

		if (ma_sound_init_from_data_source(
				MSoundManager()->GetEngine(),
				&m_rb,
				m_is3d ? 0 : MA_SOUND_FLAG_NO_SPATIALIZATION,
				NULL,
				&m_sound
			) != MA_SUCCESS) {
			goto done;
		}

		ma_sound_set_looping(&m_sound, MA_TRUE);

		SetVolume(((MxDSSound*) m_action)->GetVolume());
		ProgressTickleState(e_streaming);
		success = TRUE;

	done:
		if (!success) {
			EndAction();
		}
	}
}

// FUNCTION: LEGO1 0x100b1ea0
void MxWavePresenter::StreamingTickle()
{
	if (!m_currentChunk) {
		if (!(m_action->GetFlags() & MxDSAction::c_looping)) {
			MxStreamChunk* chunk = CurrentChunk();

			if (chunk && chunk->GetChunkFlags() & DS_CHUNK_END_OF_STREAM &&
				!(chunk->GetChunkFlags() & DS_CHUNK_BIT16)) {
				chunk->SetChunkFlags(chunk->GetChunkFlags() | DS_CHUNK_BIT16);

				m_currentChunk = new MxStreamChunk;
				MxU8* data = new MxU8[m_chunkLength];

				memset(data, m_silenceData, m_chunkLength);

				m_currentChunk->SetLength(m_chunkLength);
				m_currentChunk->SetData(data);
				m_currentChunk->SetTime(chunk->GetTime() + 1000);
				m_currentChunk->SetChunkFlags(DS_CHUNK_BIT1);
			}
		}

		MxMediaPresenter::StreamingTickle();
	}
}

void MxWavePresenter::RepeatingTickle()
{
	if (IsEnabled() && !m_currentChunk) {
		if (m_action->GetElapsedTime() >= m_action->GetStartTime() + m_action->GetDuration()) {
			ProgressTickleState(e_freezing);
		}
	}
}

// FUNCTION: LEGO1 0x100b20c0
void MxWavePresenter::DoneTickle()
{
	if (!ma_sound_get_engine(&m_sound) || m_action->GetFlags() & MxDSAction::c_bit7 ||
		ma_pcm_rb_pointer_distance(&m_rb) == 0) {
		MxMediaPresenter::DoneTickle();
	}
}

// FUNCTION: LEGO1 0x100b2130
void MxWavePresenter::LoopChunk(MxStreamChunk* p_chunk)
{
	// [library:audio]
	// The original code writes all the chunks directly into the buffer. However, since we are using
	// a ring buffer instead, we cannot do that. Instead, we use the original code's `m_loopingChunks`
	// to store permanent copies of all the chunks. (`MxSoundPresenter::LoopChunk`)
	// These will then be used to write them all at once to the ring buffer when necessary.
	MxSoundPresenter::LoopChunk(p_chunk);

	assert(m_action->GetFlags() & MxDSAction::c_looping);

	// [library:audio]
	// We don't currently support a loop count greater than 1 for repeating actions.
	// However, there don't seem to be any such actions in the game.
	assert(m_action->GetLoopCount() == 1);

	// [library:audio]
	// So far there are no known cases where the sound is initially enabled if it's set to repeat.
	// While we can technically support this (see branch below), this should be tested.
	assert(!IsEnabled());

	if (IsEnabled()) {
		WriteToSoundBuffer(p_chunk->GetData(), p_chunk->GetLength());
		m_subscriber->FreeDataChunk(p_chunk);
		m_currentChunk = NULL;
	}
}

// FUNCTION: LEGO1 0x100b2160
MxResult MxWavePresenter::PutData()
{
	AUTOLOCK(m_criticalSection);

	if (IsEnabled()) {
		switch (m_currentTickleState) {
		case e_streaming:
			if (m_currentChunk && WriteToSoundBuffer(m_currentChunk->GetData(), m_currentChunk->GetLength())) {
				m_subscriber->FreeDataChunk(m_currentChunk);
				m_currentChunk = NULL;
			}

			if (!m_started) {
				if (ma_sound_start(&m_sound) == MA_SUCCESS) {
					m_started = TRUE;
				}
			}
			break;
		case e_repeating:
			if (m_started) {
				break;
			}

			assert(!ma_sound_is_playing(&m_sound));

			// [library:audio]
			// We push all the repeating chunks at once into the buffer.
			// This should never fail, since the buffer is ensured to be large enough to contain the entire sound.
			while (m_loopingChunkCursor->Next(m_currentChunk)) {
				assert(WriteToSoundBuffer(m_currentChunk->GetData(), m_currentChunk->GetLength()));
			}

			m_currentChunk = NULL;

			if (ma_sound_start(&m_sound) == MA_SUCCESS) {
				m_started = TRUE;
			}
		}
	}

	return SUCCESS;
}

// FUNCTION: LEGO1 0x100b2280
void MxWavePresenter::EndAction()
{
	if (m_action) {
		AUTOLOCK(m_criticalSection);
		MxMediaPresenter::EndAction();

		if (ma_sound_get_engine(&m_sound)) {
			ma_sound_stop(&m_sound);
		}
	}
}

// FUNCTION: LEGO1 0x100b2300
void MxWavePresenter::SetVolume(MxS32 p_volume)
{
	m_criticalSection.Enter();

	m_volume = p_volume;
	if (ma_sound_get_engine(&m_sound)) {
		MxS32 volume = p_volume * MxOmni::GetInstance()->GetSoundManager()->GetVolume() / 100;
		float attenuation = MxOmni::GetInstance()->GetSoundManager()->GetAttenuation(volume);
		ma_sound_set_volume(&m_sound, attenuation);
	}

	m_criticalSection.Leave();
}

// FUNCTION: LEGO1 0x100b2360
void MxWavePresenter::Enable(MxBool p_enable)
{
	if (IsEnabled() != p_enable) {
		MxSoundPresenter::Enable(p_enable);

		if (p_enable) {
			m_started = FALSE;
		}
		else if (ma_sound_get_engine(&m_sound)) {
			ma_sound_stop(&m_sound);
		}
	}
}

// FUNCTION: LEGO1 0x100b23a0
void MxWavePresenter::ParseExtra()
{
	MxSoundPresenter::ParseExtra();

	MxU16 extraLength;
	char* extraData;
	m_action->GetExtra(extraLength, extraData);

	if (extraLength) {
		char extraCopy[512];
		memcpy(extraCopy, extraData, extraLength);
		extraCopy[extraLength] = '\0';

		char soundValue[512];
		if (KeyValueStringParse(soundValue, g_strSOUND, extraCopy)) {
			if (!strcmpi(soundValue, "FALSE")) {
				Enable(FALSE);
			}
		}
	}
}

// FUNCTION: LEGO1 0x100b2440
void MxWavePresenter::Pause()
{
	if (!m_paused && m_started) {
		if (ma_sound_get_engine(&m_sound)) {
			ma_sound_stop(&m_sound);
		}
		m_paused = TRUE;
	}
}

// FUNCTION: LEGO1 0x100b2470
void MxWavePresenter::Resume()
{
	if (m_paused) {
		if (ma_sound_get_engine(&m_sound) && m_started) {
			ma_sound_start(&m_sound);
		}

		m_paused = FALSE;
	}
}
