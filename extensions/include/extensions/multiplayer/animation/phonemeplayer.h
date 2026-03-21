#pragma once

#include "extensions/multiplayer/animation/loader.h"

#include <cstdint>
#include <vector>

class LegoROI;
class LegoTextureInfo;
class MxBitmap;

namespace Multiplayer::Animation
{

struct PhonemeState {
	LegoROI* targetROI;
	LegoTextureInfo* originalTexture;
	LegoTextureInfo* cachedTexture;
	MxBitmap* bitmap;
	int32_t currentFrame;
};

class PhonemePlayer {
public:
	void Init(const std::vector<SceneAnimData::PhonemeTrack>& p_tracks, LegoROI** p_roiMap, MxU32 p_roiMapSize);
	void Tick(float p_elapsedMs, const std::vector<SceneAnimData::PhonemeTrack>& p_tracks);
	void Cleanup();

private:
	std::vector<PhonemeState> m_states;
};

} // namespace Multiplayer::Animation
