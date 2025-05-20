#ifndef MXWAVEPRESENTER_H
#define MXWAVEPRESENTER_H

#include "decomp.h"
#include "mxminiaudio.h"
#include "mxsoundpresenter.h"

// VTABLE: LEGO1 0x100d49a8
// SIZE 0x6c
class MxWavePresenter : public MxSoundPresenter {
public:
	MxWavePresenter() { Init(); }

	// FUNCTION: LEGO1 0x1000d640
	~MxWavePresenter() override { Destroy(TRUE); } // vtable+0x00

	// FUNCTION: LEGO1 0x1000d6a0
	void Destroy() override { Destroy(FALSE); } // vtable+0x38

	virtual void Pause();  // vtable+0x64
	virtual void Resume(); // vtable+0x68

	// FUNCTION: LEGO1 0x1000d6b0
	virtual MxBool IsPaused() { return m_paused; } // vtable+0x6c

	// FUNCTION: BETA10 0x1008cd00
	static const char* HandlerClassName()
	{
		// STRING: LEGO1 0x100f07b4
		return "MxWavePresenter";
	}

	// FUNCTION: LEGO1 0x1000d6c0
	// FUNCTION: BETA10 0x1008ccd0
	const char* ClassName() const override // vtable+0x0c
	{
		return HandlerClassName();
	}

	// FUNCTION: LEGO1 0x1000d6d0
	MxBool IsA(const char* p_name) const override // vtable+0x10
	{
		return !strcmp(p_name, MxWavePresenter::ClassName()) || MxSoundPresenter::IsA(p_name);
	}

	void ReadyTickle() override;                     // vtable+0x18
	void StartingTickle() override;                  // vtable+0x1c
	void StreamingTickle() override;                 // vtable+0x20
	void DoneTickle() override;                      // vtable+0x2c
	void ParseExtra() override;                      // vtable+0x30
	MxResult AddToManager() override;                // vtable+0x34
	void EndAction() override;                       // vtable+0x40
	MxResult PutData() override;                     // vtable+0x4c
	void Enable(MxBool p_enable) override;           // vtable+0x54
	void LoopChunk(MxStreamChunk* p_chunk) override; // vtable+0x58
	void SetVolume(MxS32 p_volume) override;         // vtable+0x60

#pragma pack(push, 1)
	// SIZE 0x18
	struct WaveFormat {
		MxU16 m_formatTag;      // 0x00
		MxU16 m_channels;       // 0x02
		MxU32 m_samplesPerSec;  // 0x04
		MxU32 m_avgBytesPerSec; // 0x08
		MxU16 m_blockAlign;     // 0x0c
		MxU16 m_bitsPerSample;  // 0x0e
		MxU32 m_dataSize;       // 0x10
		MxU32 m_flags;          // 0x14
	};
#pragma pack(pop)

	// SYNTHETIC: LEGO1 0x1000d810
	// MxWavePresenter::`scalar deleting destructor'

protected:
	void Init();
	void Destroy(MxBool p_fromDestructor);
	MxBool WriteToSoundBuffer(void* p_audioPtr, MxU32 p_length);

	// [library:audio] One chunk has up to 1 second worth of frames
	static const MxU32 g_millisecondsPerChunk = 1000;

	// [library:audio] Store up to 2 chunks worth of frames (same as in original game)
	static const MxU32 g_rbSizeInMilliseconds = g_millisecondsPerChunk * 2;

	// [library:audio] WAVE_FORMAT_PCM (audio in .SI files only used this format)
	static const MxU32 g_supportedFormatTag = 1;

	WaveFormat* m_waveFormat; // 0x54

	// [library:audio]
	// If MxDSAction::looping is set, we keep the entire audio in memory and use `m_ab`.
	// In (most) other cases, data is streamed through the ring buffer `m_rb`.
	MxMiniaudio<ma_pcm_rb> m_rb;
	struct {
		MxMiniaudio<ma_audio_buffer> m_buffer;
		MxU8* m_data;
		MxU32 m_length;
		MxU32 m_offset;
	} m_ab;

	MxMiniaudio<ma_sound> m_sound;
	MxU32 m_chunkLength; // 0x5c
	MxBool m_started;    // 0x65
	MxBool m_is3d;       // 0x66
	MxS8 m_silenceData;  // 0x67
	MxBool m_paused;     // 0x68
};

#endif // MXWAVEPRESENTER_H
