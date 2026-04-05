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

// Loads animation data from SI files on demand.
// Supports multiple worlds' SI files (isle.si, act2main.si, act3.si).
class Loader {
public:
	Loader();
	~Loader();

	void SetSIReader(SIReader* p_reader) { m_reader = p_reader; }

	SceneAnimData* EnsureCached(int8_t p_worldId, uint32_t p_objectId);
	void PreloadAsync(int8_t p_preloadWorldId, uint32_t p_preloadObjectId);

	// Get the SI file path for a world. Returns nullptr if unsupported.
	static const char* GetSIPath(int8_t p_worldId);

private:
	class PreloadThread : public MxThread {
	public:
		PreloadThread(Loader* p_loader, int8_t p_worldId, uint32_t p_objectId);
		MxResult Run() override;

	private:
		Loader* m_loader;
		int8_t m_worldId;
		uint32_t m_objectId;
	};

	// SI file handle for non-act1 worlds (act1 uses the external SIReader).
	struct SIHandle {
		si::File* file;
		si::Interleaf* interleaf;
		bool ready;
	};

	bool OpenWorldSI(int8_t p_worldId);
	bool ReadWorldObject(int8_t p_worldId, uint32_t p_objectId, si::Object*& p_outObj);

	static bool ParseAnimationChild(si::Object* p_child, SceneAnimData& p_data);
	static bool ParsePhonemeChild(si::Object* p_child, SceneAnimData& p_data);
	static bool ParseComposite(si::Object* p_composite, SceneAnimData& p_data);
	void CleanupPreloadThread();

	static uint64_t CacheKey(int8_t p_worldId, uint32_t p_objectId)
	{
		return (uint64_t((uint8_t) p_worldId) << 32) | p_objectId;
	}

	SIReader* m_reader;                        // external reader for isle.si (act1)
	std::map<int8_t, SIHandle> m_extraSI;      // SI handles for non-act1 worlds
	std::map<uint64_t, SceneAnimData> m_cache; // keyed by CacheKey(worldId, objectId)
	MxCriticalSection m_cacheCS;

	PreloadThread* m_preloadThread;
	int8_t m_preloadWorldId;
	uint32_t m_preloadObjectId;
	std::atomic<bool> m_preloadDone;
};

} // namespace Multiplayer::Animation
