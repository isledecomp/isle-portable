#include "mxsoundmanager.h"

#include "mxautolock.h"
#include "mxdsaction.h"
#include "mxmain.h"
#include "mxmisc.h"
#include "mxpresenter.h"
#include "mxticklemanager.h"
#include "mxticklethread.h"
#include "mxwavepresenter.h"

#include <SDL3/SDL_log.h>

DECOMP_SIZE_ASSERT(MxSoundManager, 0x3c);

// GLOBAL LEGO1 0x10101420
MxS32 g_volumeAttenuation[100] = {-6643, -5643, -5058, -4643, -4321, -4058, -3836, -3643, -3473, -3321, -3184, -3058,
								  -2943, -2836, -2736, -2643, -2556, -2473, -2395, -2321, -2251, -2184, -2120, -2058,
								  -2000, -1943, -1888, -1836, -1785, -1736, -1689, -1643, -1599, -1556, -1514, -1473,
								  -1434, -1395, -1358, -1321, -1286, -1251, -1217, -1184, -1152, -1120, -1089, -1058,
								  -1029, -1000, -971,  -943,  -915,  -888,  -862,  -836,  -810,  -785,  -761,  -736,
								  -713,  -689,  -666,  -643,  -621,  -599,  -577,  -556,  -535,  -514,  -494,  -473,
								  -454,  -434,  -415,  -395,  -377,  -358,  -340,  -321,  -304,  -286,  -268,  -251,
								  -234,  -217,  -200,  -184,  -168,  -152,  -136,  -120,  -104,  -89,   -74,   -58,
								  -43,   -29,   -14,   0};

// FUNCTION: LEGO1 0x100ae740
// FUNCTION: BETA10 0x10132c70
MxSoundManager::MxSoundManager()
{
	Init();
}

// FUNCTION: LEGO1 0x100ae7d0
// FUNCTION: BETA10 0x10132ce7
MxSoundManager::~MxSoundManager()
{
	Destroy(TRUE);
}

// FUNCTION: LEGO1 0x100ae830
// FUNCTION: BETA10 0x10132d59
void MxSoundManager::Init()
{
	SDL_zero(m_engine);
	m_stream = NULL;
}

// FUNCTION: LEGO1 0x100ae840
// FUNCTION: BETA10 0x10132d89
void MxSoundManager::Destroy(MxBool p_fromDestructor)
{
	if (m_thread) {
		m_thread->Terminate();
		delete m_thread;
	}
	else {
		TickleManager()->UnregisterClient(this);
	}

	ENTER(m_criticalSection);

	if (m_stream) {
		SDL_DestroyAudioStream(m_stream);
	}

	m_engine.Destroy(ma_engine_uninit);

	Init();
	m_criticalSection.Leave();

	if (!p_fromDestructor) {
		MxAudioManager::Destroy();
	}
}

// FUNCTION: LEGO1 0x100ae8b0
// FUNCTION: BETA10 0x10132e94
MxResult MxSoundManager::Create(MxU32 p_frequencyMS, MxBool p_createThread)
{
	MxResult status = FAILURE;
	MxBool locked = FALSE;
	ma_engine_config engineConfig;

	if (MxAudioManager::Create() != SUCCESS) {
		goto done;
	}

	ENTER(m_criticalSection);
	locked = TRUE;

	engineConfig = ma_engine_config_init();
	engineConfig.noDevice = MA_TRUE;
	engineConfig.channels = MxOmni::IsSound3D() ? 2 : 1;
	engineConfig.sampleRate = g_sampleRate;

	if (m_engine.Init(ma_engine_init, &engineConfig) != MA_SUCCESS) {
		goto done;
	}

	SDL_AudioSpec spec;
	SDL_zero(spec);
	spec.freq = ma_engine_get_sample_rate(m_engine);
	spec.format = SDL_AUDIO_F32;
	spec.channels = ma_engine_get_channels(m_engine);

	if ((m_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &AudioStreamCallback, this)) !=
		NULL) {
		SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(m_stream));
	}
	else {
		SDL_LogError(
			SDL_LOG_CATEGORY_APPLICATION,
			"Failed to open default audio device for playback: %s",
			SDL_GetError()
		);
	}

	if (p_createThread) {
		m_thread = new MxTickleThread(this, p_frequencyMS);

		if (!m_thread || m_thread->Start(0, 0) != SUCCESS) {
			goto done;
		}
	}
	else {
		TickleManager()->RegisterClient(this, p_frequencyMS);
	}

	status = SUCCESS;

done:
	if (status != SUCCESS) {
		Destroy();
	}

	if (locked) {
		m_criticalSection.Leave();
	}
	return status;
}

void MxSoundManager::AudioStreamCallback(
	void* p_userdata,
	SDL_AudioStream* p_stream,
	int p_additionalAmount,
	int p_totalAmount
)
{
	static vector<MxU8> g_buffer;
	g_buffer.reserve(p_additionalAmount);

	MxSoundManager* manager = (MxSoundManager*) p_userdata;
	ma_uint32 bytesPerFrame = ma_get_bytes_per_frame(ma_format_f32, ma_engine_get_channels(manager->m_engine));
	ma_uint32 bufferSizeInFrames = (ma_uint32) p_additionalAmount / bytesPerFrame;
	ma_uint64 framesRead;

	if (ma_engine_read_pcm_frames(manager->m_engine, g_buffer.data(), bufferSizeInFrames, &framesRead) == MA_SUCCESS) {
		SDL_PutAudioStreamData(manager->m_stream, g_buffer.data(), framesRead * bytesPerFrame);
	}
}

// FUNCTION: LEGO1 0x100aeab0
// FUNCTION: BETA10 0x101331e3
void MxSoundManager::Destroy()
{
	Destroy(FALSE);
}

// FUNCTION: LEGO1 0x100aeac0
// FUNCTION: BETA10 0x10133203
void MxSoundManager::SetVolume(MxS32 p_volume)
{
	MxAudioManager::SetVolume(p_volume);

	ENTER(m_criticalSection);

	MxPresenter* presenter;
	MxPresenterListCursor cursor(m_presenters);

	while (cursor.Next(presenter)) {
		((MxAudioPresenter*) presenter)->SetVolume(((MxAudioPresenter*) presenter)->GetVolume());
	}

	m_criticalSection.Leave();
}

// FUNCTION: LEGO1 0x100aebd0
// FUNCTION: BETA10 0x101332cf
MxPresenter* MxSoundManager::FindPresenter(const MxAtomId& p_atomId, MxU32 p_objectId)
{
	AUTOLOCK(m_criticalSection);

	MxPresenter* presenter;
	MxPresenterListCursor cursor(m_presenters);

	while (cursor.Next(presenter)) {
		if (presenter->GetAction()->GetAtomId() == p_atomId && presenter->GetAction()->GetObjectId() == p_objectId) {
			return presenter;
		}
	}

	return NULL;
}

// FUNCTION: LEGO1 0x100aecf0
float MxSoundManager::GetAttenuation(MxU32 p_volume)
{
	// [library:audio] Convert DSOUND attenutation units to linear miniaudio volume

	if (p_volume == 0) {
		return 0.0f;
	}

	return ma_volume_db_to_linear((float) g_volumeAttenuation[p_volume - 1] / 100.0f);
}

// FUNCTION: LEGO1 0x100aed10
void MxSoundManager::Pause()
{
	AUTOLOCK(m_criticalSection);

	MxPresenter* presenter;
	MxPresenterListCursor cursor(m_presenters);

	while (cursor.Next(presenter)) {
		if (presenter->IsA("MxWavePresenter")) {
			((MxWavePresenter*) presenter)->Pause();
		}
	}
}

// FUNCTION: LEGO1 0x100aee10
void MxSoundManager::Resume()
{
	AUTOLOCK(m_criticalSection);

	MxPresenter* presenter;
	MxPresenterListCursor cursor(m_presenters);

	while (cursor.Next(presenter)) {
		if (presenter->IsA("MxWavePresenter")) {
			((MxWavePresenter*) presenter)->Resume();
		}
	}
}
