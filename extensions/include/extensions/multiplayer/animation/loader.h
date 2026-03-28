#pragma once

#include "extensions/multiplayer/sireader.h"
#include "mxcriticalsection.h"
#include "mxthread.h"
#include "mxwavepresenter.h"

#include <atomic>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct FLIC_HEADER;
class LegoAnim;

namespace si
{
class Object;
} // namespace si

namespace Multiplayer::Animation
{

struct SceneAnimData {
	LegoAnim* anim;
	float duration;

	using AudioTrack = Multiplayer::AudioTrack;
	std::vector<AudioTrack> audioTracks;

	struct PhonemeTrack {
		FLIC_HEADER* flcHeader;
		std::vector<std::vector<char>> frameData;
		uint32_t timeOffset;
		std::string roiName;
		uint16_t width, height;
	};
	std::vector<PhonemeTrack> phonemeTracks;

	// Action transform from SI metadata (location/direction/up)
	struct {
		float location[3];
		float direction[3];
		float up[3];
		bool valid;
	} actionTransform;

	std::vector<std::string> ptAtCamNames; // ROI names from PTATCAM directive
	bool hideOnStop;

	SceneAnimData();
	~SceneAnimData();

	SceneAnimData(const SceneAnimData&) = delete;
	SceneAnimData& operator=(const SceneAnimData&) = delete;
	SceneAnimData(SceneAnimData&& p_other) noexcept;
	SceneAnimData& operator=(SceneAnimData&& p_other) noexcept;

private:
	void ReleaseTracks();
};

// Loads animation data from ISLE.SI on demand.
// Delegates SI file access to a SIReader instance.
class Loader {
public:
	Loader();
	~Loader();

	void SetSIReader(SIReader* p_reader) { m_reader = p_reader; }

	SceneAnimData* EnsureCached(uint32_t p_objectId);
	void PreloadAsync(uint32_t p_objectId);

private:
	class PreloadThread : public MxThread {
	public:
		PreloadThread(Loader* p_loader, uint32_t p_objectId);
		MxResult Run() override;

	private:
		Loader* m_loader;
		uint32_t m_objectId;
	};

	static bool ParseAnimationChild(si::Object* p_child, SceneAnimData& p_data);
	static bool ParsePhonemeChild(si::Object* p_child, SceneAnimData& p_data);
	static bool ParseComposite(si::Object* p_composite, SceneAnimData& p_data);
	void CleanupPreloadThread();

	SIReader* m_reader;
	std::map<uint32_t, SceneAnimData> m_cache;
	MxCriticalSection m_cacheCS;

	PreloadThread* m_preloadThread;
	uint32_t m_preloadObjectId;
	std::atomic<bool> m_preloadDone;
};

} // namespace Multiplayer::Animation
