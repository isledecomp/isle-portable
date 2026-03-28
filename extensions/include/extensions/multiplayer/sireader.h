#pragma once

#include "mxcriticalsection.h"
#include "mxwavepresenter.h"

#include <cstdint>
#include <string>

namespace si
{
class File;
class Interleaf;
class Object;
} // namespace si

namespace Multiplayer
{

struct AudioTrack {
	MxU8* pcmData;
	MxU32 pcmDataSize;
	MxWavePresenter::WaveFormat format;
	std::string mediaSrcPath;
	int32_t volume;
	uint32_t timeOffset;
};

// Reads objects from an SI archive on demand, bypassing the streaming pipeline.
// Reads only the RIFF header + offset table on first open, then seeks to
// individual objects as requested.
class SIReader {
public:
	SIReader();
	~SIReader();

	// Open isle.si with header-only read (lazy object loading)
	bool Open();

	// Open any SI file with header-only read (for background threads with independent handles)
	static bool OpenHeaderOnly(const char* p_siPath, si::File*& p_file, si::Interleaf*& p_interleaf);

	// Lazy-load a single SI object by index
	bool ReadObject(uint32_t p_objectId);

	// Get a previously loaded object (must call ReadObject first)
	si::Object* GetObject(uint32_t p_objectId);

	// Extract WAV audio from a single SI child object
	static bool ExtractAudioTrack(si::Object* p_child, AudioTrack& p_out);

	// Extract the first WAV child from a composite SI object.
	// Opens SI if needed, reads the object, finds first WAV child, extracts audio.
	// Caller owns the returned pointer and its pcmData buffer.
	AudioTrack* ExtractFirstAudio(uint32_t p_objectId);

	bool IsReady() const { return m_siReady; }
	si::Interleaf* GetInterleaf() { return m_interleaf; }

private:
	si::File* m_siFile;
	si::Interleaf* m_interleaf;
	bool m_siReady;
	MxCriticalSection m_cs;
};

} // namespace Multiplayer
