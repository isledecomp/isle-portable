#include "extensions/multiplayer/animation/audioplayer.h"

#include "extensions/multiplayer/animation/loader.h"
#include "legocachsound.h"

using namespace Multiplayer::Animation;

void AudioPlayer::Init(const std::vector<SceneAnimData::AudioTrack>& p_tracks)
{
	for (const auto& audioTrack : p_tracks) {
		LegoCacheSound* sound = new LegoCacheSound();
		MxString mediaSrcPath(audioTrack.mediaSrcPath.c_str());
		MxWavePresenter::WaveFormat format = audioTrack.format;
		if (sound->Create(format, mediaSrcPath, audioTrack.volume, audioTrack.pcmData, audioTrack.pcmDataSize) ==
			SUCCESS) {
			// Disable Doppler on extension-created sounds. Camera animations drive high
			// listener velocities via CalculateWorldVelocity, and miniaudio's default
			// dopplerFactor of 1.0 shifts the pitch/speed of spatialized sounds.
			ma_sound_set_doppler_factor(sound->m_cacheSound, 0);

			ActiveSound active;
			active.sound = sound;
			active.timeOffset = audioTrack.timeOffset;
			active.started = false;
			m_activeSounds.push_back(active);
		}
		else {
			delete sound;
		}
	}
}

void AudioPlayer::Tick(float p_elapsedMs, const char* p_roiName)
{
	for (auto& active : m_activeSounds) {
		if (!active.started && p_elapsedMs >= (float) active.timeOffset) {
			active.sound->Play(p_roiName, FALSE);
			active.started = true;
		}
		if (active.started) {
			active.sound->FUN_10006be0();
		}
	}
}

void AudioPlayer::Cleanup()
{
	for (auto& active : m_activeSounds) {
		if (active.started) {
			active.sound->Stop();
		}
		delete active.sound;
	}
	m_activeSounds.clear();
}
