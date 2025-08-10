#include "mxwavepresenter.h"

#include "decomp.h"
#include "define.h"
#include "mxautolock.h"
#include "mxdssound.h"
#include "mxdssubscriber.h"
#include "mxmain.h"
#include "mxmisc.h"
#include "mxsoundmanager.h"
#include "mxutilities.h"

#include <SDL3/SDL_stdinc.h>
#include <assert.h>

DECOMP_SIZE_ASSERT(MxWavePresenter, 0x6c);
DECOMP_SIZE_ASSERT(MxWavePresenter::WaveFormat, 0x18);

// FUNCTION: LEGO1 0x100b1ad0
void MxWavePresenter::Init()
{
	m_waveFormat = NULL;
	SDL_zero(m_rb);
	SDL_zero(m_ab);
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
	m_sound.Destroy(ma_sound_uninit);
	m_rb.Destroy(ma_pcm_rb_uninit);
	m_ab.m_buffer.Destroy(ma_audio_buffer_uninit);
	delete[] m_ab.m_data;

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
	if (m_action->IsLooping()) {
		assert(m_ab.m_offset + p_length <= m_ab.m_length);
		memcpy(m_ab.m_data + m_ab.m_offset, p_audioPtr, p_length);
		m_ab.m_offset += p_length;
		return TRUE;
	}
	else {
		ma_uint32 requestedFrames =
			ma_calculate_buffer_size_in_frames_from_milliseconds(g_millisecondsPerChunk, m_waveFormat->m_samplesPerSec);
		ma_uint32 acquiredFrames = requestedFrames;
		void* bufferOut;

		ma_pcm_rb_acquire_write(m_rb, &acquiredFrames, &bufferOut);

		// [library:audio] If there isn't enough space in the buffer for a full chunk, try again later.
		if (acquiredFrames != requestedFrames) {
			ma_pcm_rb_commit_write(m_rb, 0);
			return FALSE;
		}

		ma_uint32 acquiredBytes = acquiredFrames * ma_get_bytes_per_frame(m_rb->format, m_rb->channels);
		assert(p_length <= acquiredBytes);

		memcpy(bufferOut, p_audioPtr, p_length);

		// [library:audio] Pad with silence data if we don't have a full chunk.
		if (p_length < acquiredBytes) {
			memset((ma_uint8*) bufferOut + p_length, m_silenceData, acquiredBytes - p_length);
		}

		ma_pcm_rb_commit_write(m_rb, acquiredFrames);
		return TRUE;
	}
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

		ma_format format = m_waveFormat->m_bitsPerSample == 16 ? ma_format_s16 : ma_format_u8;
		ma_uint32 channels = m_waveFormat->m_channels;
		ma_uint32 sampleRate = m_waveFormat->m_samplesPerSec;

		if (m_action->IsLooping()) {
			ma_uint32 sizeInFrames = ma_calculate_buffer_size_in_frames_from_milliseconds(
				m_action->GetDuration() / m_action->GetLoopCount(),
				sampleRate
			);

			m_ab.m_length = ma_get_bytes_per_frame(format, channels) * sizeInFrames;
			m_ab.m_data = new MxU8[m_ab.m_length];

			ma_audio_buffer_config config =
				ma_audio_buffer_config_init(format, channels, sizeInFrames, m_ab.m_data, NULL);
			config.sampleRate = sampleRate;

			if (m_ab.m_buffer.Init(ma_audio_buffer_init, &config) != MA_SUCCESS) {
				goto done;
			}
		}
		else {
			if (m_rb.Init(
					ma_pcm_rb_init,
					format,
					channels,
					ma_calculate_buffer_size_in_frames_from_milliseconds(g_rbSizeInMilliseconds, sampleRate),
					nullptr,
					nullptr
				) != MA_SUCCESS) {
				goto done;
			}

			ma_pcm_rb_set_sample_rate(m_rb, sampleRate);
		}

		if (m_sound.Init(
				ma_sound_init_from_data_source,
				MSoundManager()->GetEngine(),
				m_action->IsLooping() ? (ma_data_source*) m_ab.m_buffer : (ma_data_source*) m_rb,
				m_is3d ? 0 : MA_SOUND_FLAG_NO_SPATIALIZATION,
				nullptr
			) != MA_SUCCESS) {
			goto done;
		}

		// [library:audio]
		// There is an issue with certain spatialized sounds causing an audio glitch.
		// To temporarily resolve this, we can disable the Doppler effect.
		// More info: https://github.com/mackron/miniaudio/issues/885
		ma_sound_set_doppler_factor(m_sound, 0.0f);
		ma_sound_set_looping(m_sound, m_action->IsLooping() ? m_action->GetLoopCount() > 1 : MA_TRUE);

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

// FUNCTION: LEGO1 0x100b20c0
void MxWavePresenter::DoneTickle()
{
	if (!m_sound || m_action->GetFlags() & MxDSAction::c_bit7 || m_action->GetFlags() & MxDSAction::c_looping ||
		ma_pcm_rb_pointer_distance(m_rb) == 0) {
		MxMediaPresenter::DoneTickle();
	}
}

// FUNCTION: LEGO1 0x100b2130
void MxWavePresenter::LoopChunk(MxStreamChunk* p_chunk)
{
	WriteToSoundBuffer(p_chunk->GetData(), p_chunk->GetLength());
	if (IsEnabled()) {
		m_subscriber->FreeDataChunk(p_chunk);
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
				if (ma_sound_start(m_sound) == MA_SUCCESS) {
					m_started = TRUE;
				}
			}
			break;
		case e_repeating:
			if (m_started) {
				break;
			}

			assert(!ma_sound_is_playing(m_sound));
			ma_sound_seek_to_pcm_frame(m_sound, 0);

			if (ma_sound_start(m_sound) == MA_SUCCESS) {
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

		if (m_sound) {
			ma_sound_stop(m_sound);
		}
	}
}

// FUNCTION: LEGO1 0x100b2300
void MxWavePresenter::SetVolume(MxS32 p_volume)
{
	ENTER(m_criticalSection);

	m_volume = p_volume;
	if (m_sound) {
		MxS32 volume = p_volume * MxOmni::GetInstance()->GetSoundManager()->GetVolume() / 100;
		float attenuation = MxOmni::GetInstance()->GetSoundManager()->GetAttenuation(volume);
		ma_sound_set_volume(m_sound, attenuation);
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
		else if (m_sound) {
			ma_sound_stop(m_sound);
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
			if (!SDL_strcasecmp(soundValue, "FALSE")) {
				Enable(FALSE);
			}
		}
	}
}

// FUNCTION: LEGO1 0x100b2440
void MxWavePresenter::Pause()
{
	if (!m_paused && m_started) {
		if (m_sound) {
			ma_sound_stop(m_sound);
		}
		m_paused = TRUE;
	}
}

// FUNCTION: LEGO1 0x100b2470
void MxWavePresenter::Resume()
{
	if (m_paused) {
		if (m_sound && m_started) {
			switch (m_currentTickleState) {
			case e_streaming:
			case e_repeating:
				ma_sound_start(m_sound);
				break;
			case e_done:
				if (!ma_sound_at_end(m_sound)) {
					ma_sound_start(m_sound);
				}
			}
		}

		m_paused = FALSE;
	}
}
