#include "legocachsound.h"

#include "legosoundmanager.h"
#include "misc.h"
#include "mxmain.h"

#include <assert.h>

DECOMP_SIZE_ASSERT(LegoCacheSound, 0x88)

// FUNCTION: LEGO1 0x100064d0
// FUNCTION: BETA10 0x10066340
LegoCacheSound::LegoCacheSound()
{
	Init();
}

// FUNCTION: LEGO1 0x10006630
// FUNCTION: BETA10 0x100663f3
LegoCacheSound::~LegoCacheSound()
{
	Destroy();
}

// FUNCTION: LEGO1 0x100066d0
// FUNCTION: BETA10 0x10066498
void LegoCacheSound::Init()
{
	SDL_zero(m_buffer);
	SDL_zero(m_cacheSound);
	m_data = NULL;
	m_unk0x58 = FALSE;
	memset(&m_wfx, 0, sizeof(m_wfx));
	m_looping = TRUE;
	m_unk0x6a = FALSE;
	m_volume = 79;
	m_unk0x70 = FALSE;
	m_muted = FALSE;
}

// FUNCTION: LEGO1 0x10006710
// FUNCTION: BETA10 0x10066505
MxResult LegoCacheSound::Create(
	MxWavePresenter::WaveFormat& p_pwfx,
	MxString p_mediaSrcPath,
	MxS32 p_volume,
	MxU8* p_data,
	MxU32 p_dataSize
)
{
	// [library:audio] These should never be null
	assert(p_data != NULL && p_dataSize != 0);

	assert(p_pwfx.m_formatTag == g_supportedFormatTag);
	assert(p_pwfx.m_bitsPerSample == 8 || p_pwfx.m_bitsPerSample == 16);

	CopyData(p_data, p_dataSize);

	ma_format format = p_pwfx.m_bitsPerSample == 16 ? ma_format_s16 : ma_format_u8;
	ma_uint32 bytesPerFrame = ma_get_bytes_per_frame(format, p_pwfx.m_channels);
	ma_uint32 bufferSizeInFrames = p_dataSize / bytesPerFrame;
	ma_audio_buffer_config config =
		ma_audio_buffer_config_init(format, p_pwfx.m_channels, bufferSizeInFrames, m_data, NULL);
	config.sampleRate = p_pwfx.m_samplesPerSec;

	if (m_buffer.Init(ma_audio_buffer_init, &config) != MA_SUCCESS) {
		return FAILURE;
	}

	if (m_cacheSound.Init(
			ma_sound_init_from_data_source,
			SoundManager()->GetEngine(),
			&m_buffer,
			MxOmni::IsSound3D() ? 0 : MA_SOUND_FLAG_NO_SPATIALIZATION,
			nullptr
		) != MA_SUCCESS) {
		return FAILURE;
	}

	m_volume = p_volume;

	MxS32 volume = m_volume * SoundManager()->GetVolume() / 100;
	ma_sound_set_volume(m_cacheSound, SoundManager()->GetAttenuation(volume));

	if (m_sound.Create(m_cacheSound, NULL, m_volume) != SUCCESS) {
		return FAILURE;
	}

	m_unk0x48 = GetBaseFilename(p_mediaSrcPath);
	m_wfx = p_pwfx;
	return SUCCESS;
}

// FUNCTION: LEGO1 0x100068e0
// FUNCTION: BETA10 0x100667a0
void LegoCacheSound::CopyData(MxU8* p_data, MxU32 p_dataSize)
{
	assert(p_data);
	assert(p_dataSize);

	delete[] m_data;
	m_dataSize = p_dataSize;
	m_data = new MxU8[m_dataSize];
	memcpy(m_data, p_data, m_dataSize);
}

// FUNCTION: LEGO1 0x10006920
// FUNCTION: BETA10 0x1006685b
void LegoCacheSound::Destroy()
{
	m_cacheSound.Destroy(ma_sound_uninit);
	m_buffer.Destroy(ma_audio_buffer_uninit);

	delete[] m_data;
	Init();
}

// FUNCTION: LEGO1 0x10006960
// FUNCTION: BETA10 0x100668cf
LegoCacheSound* LegoCacheSound::Clone()
{
	LegoCacheSound* pnew = new LegoCacheSound();
	assert(pnew);

	MxResult result = pnew->Create(m_wfx, m_unk0x48, m_volume, m_data, m_dataSize);
	if (result == SUCCESS) {
		return pnew;
	}
	else {
		delete pnew;
		return NULL;
	}
}

// FUNCTION: LEGO1 0x10006a30
// FUNCTION: BETA10 0x10066a23
MxResult LegoCacheSound::Play(const char* p_name, MxBool p_looping)
{
	if (m_data == NULL || m_dataSize == 0) {
		return FAILURE;
	}

	m_unk0x6a = FALSE;
	m_sound.FUN_10011a60(m_cacheSound, p_name);

	if (p_name != NULL) {
		m_unk0x74 = p_name;
	}

	if (ma_sound_seek_to_pcm_frame(m_cacheSound, 0) != MA_SUCCESS) {
		return FAILURE;
	}

	ma_sound_set_looping(m_cacheSound, p_looping);

	if (ma_sound_start(m_cacheSound) != MA_SUCCESS) {
		return FAILURE;
	}

	if (p_looping == FALSE) {
		m_looping = FALSE;
	}
	else {
		m_looping = TRUE;
	}

	m_unk0x58 = TRUE;
	m_unk0x70 = TRUE;
	return SUCCESS;
}

// FUNCTION: LEGO1 0x10006b80
// FUNCTION: BETA10 0x10066ca3
void LegoCacheSound::Stop()
{
	ma_sound_stop(m_cacheSound);

	m_unk0x58 = FALSE;
	m_unk0x6a = FALSE;

	m_sound.Reset();
	if (m_unk0x74.GetLength() != 0) {
		m_unk0x74 = "";
	}
}

// FUNCTION: LEGO1 0x10006be0
// FUNCTION: BETA10 0x10066d23
void LegoCacheSound::FUN_10006be0()
{
	if (!m_looping) {
		if (m_unk0x70) {
			if (!ma_sound_is_playing(m_cacheSound)) {
				return;
			}

			m_unk0x70 = FALSE;
		}

		if (!ma_sound_is_playing(m_cacheSound)) {
			ma_sound_stop(m_cacheSound);
			m_sound.Reset();
			if (m_unk0x74.GetLength() != 0) {
				m_unk0x74 = "";
			}

			m_unk0x58 = FALSE;
			return;
		}
	}

	if (m_unk0x74.GetLength() == 0) {
		return;
	}

	if (!m_muted) {
		if (!m_sound.UpdatePosition(m_cacheSound)) {
			if (!m_unk0x6a) {
				ma_sound_stop(m_cacheSound);
				m_unk0x6a = TRUE;
			}
		}
		else if (m_unk0x6a) {
			ma_sound_start(m_cacheSound);
			m_unk0x6a = FALSE;
		}
	}
}

// FUNCTION: LEGO1 0x10006cb0
// FUNCTION: BETA10 0x10066e85
void LegoCacheSound::SetDistance(MxS32 p_min, MxS32 p_max)
{
	m_sound.SetDistance(p_min, p_max);
}

// FUNCTION: LEGO1 0x10006cd0
// FUNCTION: BETA10 0x10066eb0
void LegoCacheSound::FUN_10006cd0(undefined4, undefined4)
{
}

// FUNCTION: LEGO1 0x10006ce0
void LegoCacheSound::MuteSilence(MxBool p_muted)
{
	if (m_muted != p_muted) {
		m_muted = p_muted;

		if (m_muted) {
			ma_sound_set_volume(m_cacheSound, ma_volume_db_to_linear(-3000.0f / 100.0f));
		}
		else {
			MxS32 volume = m_volume * SoundManager()->GetVolume() / 100;
			ma_sound_set_volume(m_cacheSound, SoundManager()->GetAttenuation(volume));
		}
	}
}

// FUNCTION: LEGO1 0x10006d40
// FUNCTION: BETA10 0x10066ec8
void LegoCacheSound::MuteStop(MxBool p_muted)
{
	if (m_muted != p_muted) {
		m_muted = p_muted;

		if (m_muted) {
			ma_sound_stop(m_cacheSound);
		}
		else {
			ma_sound_start(m_cacheSound);
		}
	}
}

// FUNCTION: LEGO1 0x10006d80
// FUNCTION: BETA10 0x100670e7
MxString LegoCacheSound::GetBaseFilename(MxString& p_path)
{
	// Get the base filename from the given path
	// e.g. "Z:\Lego\Audio\test.wav" --> "test"
	char* str = p_path.GetData();

	// Start at the end of the string and work backwards.
	char* p = str + strlen(str);
	char* end = p;

	while (str != p--) {
		// If the file has an extension, we want to exclude it from the output.
		// Set this as our new end position.
		if (*p == '.') {
			end = p;
		}

		// Stop if we hit a directory or drive letter.
		if (*p == '\\') {
			break;
		}
	}

	MxString output;
	// Increment by one to shift p to the start of the filename.
	char* x = ++p;
	// If end points to the dot in filename, change it to a null terminator.
	x[end - p] = '\0';
	return output = x;
}
