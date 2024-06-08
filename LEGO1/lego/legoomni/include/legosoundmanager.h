#ifndef LEGOSOUNDMANAGER_H
#define LEGOSOUNDMANAGER_H

#include "mxsoundmanager.h"

class LegoCacheSoundManager;

// VTABLE: LEGO1 0x100d6b10
// SIZE 0x44
class LegoSoundManager : public MxSoundManager {
public:
	LegoSoundManager();
	~LegoSoundManager() override;

	MxResult Tickle() override;                                           // vtable+0x08
	void Destroy() override;                                              // vtable+0x18
	MxResult Create(MxU32 p_frequencyMS, MxBool p_createThread) override; // vtable+0x30

	// SYNTHETIC: LEGO1 0x10029920
	// LegoSoundManager::`scalar deleting destructor'

	void UpdateListener(const float* p_position, const float* p_direction, const float* p_up, const float* p_velocity);

	inline LegoCacheSoundManager* GetCacheSoundManager() { return m_cacheSoundManager; }

private:
	void Init();
	void Destroy(MxBool p_fromDestructor);

	LegoCacheSoundManager* m_cacheSoundManager; // 0x40
};

// GLOBAL: LEGO1 0x100db6d0
// IID_IDirectSound3DListener

#endif // LEGOSOUNDMANAGER_H
