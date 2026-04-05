#pragma once

#include "extensions/multiplayer/animation/loader.h"

#include <cstdint>
#include <vector>

class LegoCacheSound;

namespace Multiplayer::Animation
{

class AudioPlayer {
public:
	// Create LegoCacheSound objects from SceneAnimData's audio tracks
	void Init(const std::vector<SceneAnimData::AudioTrack>& p_tracks);

	// Start sounds whose time offset has been reached
	void Tick(float p_elapsedMs, const char* p_roiName);

	// Stop and delete all sounds
	void Cleanup();

private:
	struct ActiveSound {
		LegoCacheSound* sound;
		uint32_t timeOffset;
		bool started;
	};
	std::vector<ActiveSound> m_activeSounds;
};

} // namespace Multiplayer::Animation
