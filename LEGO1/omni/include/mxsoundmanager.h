#ifndef MXSOUNDMANAGER_H
#define MXSOUNDMANAGER_H

#include "decomp.h"
#include "mxatom.h"
#include "mxaudiomanager.h"

#include <SDL3/SDL_audio.h>
#include <miniaudio.h>

// VTABLE: LEGO1 0x100dc128
// SIZE 0x3c
class MxSoundManager : public MxAudioManager {
public:
	MxSoundManager();
	~MxSoundManager() override; // vtable+0x00

	void Destroy() override;                                             // vtable+0x18
	void SetVolume(MxS32 p_volume) override;                             // vtable+0x2c
	virtual MxResult Create(MxU32 p_frequencyMS, MxBool p_createThread); // vtable+0x30
	virtual void Pause();                                                // vtable+0x34
	virtual void Resume();                                               // vtable+0x38

	inline ma_engine* GetEngine() { return &m_engine; }

	float GetAttenuation(MxU32 p_volume);

protected:
	void Init();
	void Destroy(MxBool p_fromDestructor);
	MxPresenter* FUN_100aebd0(const MxAtomId& p_atomId, MxU32 p_objectId);

	// [library:audio]
	// Upscaling everything to 44.1KHz, since we have various sample rates throughout the game.
	// Not sure how DirectSound handles this when different buffers have different rates.
	static const MxU32 sampleRate = 44100;

	static void SDLCALL
	AudioStreamCallback(void* p_userdata, SDL_AudioStream* p_stream, int p_additionalAmount, int p_totalAmount);

	ma_engine m_engine;
	SDL_AudioStream* m_stream;
	undefined m_unk0x38[4];
};

// SYNTHETIC: LEGO1 0x100ae7b0
// MxSoundManager::`scalar deleting destructor'

#endif // MXSOUNDMANAGER_H
