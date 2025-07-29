#include "lego3dsound.h"

#include "legoactor.h"
#include "legocharactermanager.h"
#include "legosoundmanager.h"
#include "legovideomanager.h"
#include "misc.h"
#include "mxmain.h"

#include <vec.h>

DECOMP_SIZE_ASSERT(Lego3DSound, 0x30)

// FUNCTION: LEGO1 0x10011630
Lego3DSound::Lego3DSound()
{
	Init();
}

// FUNCTION: LEGO1 0x10011670
Lego3DSound::~Lego3DSound()
{
	Destroy();
}

// FUNCTION: LEGO1 0x10011680
void Lego3DSound::Init()
{
	m_sound = NULL;
	m_roi = NULL;
	m_positionROI = NULL;
	m_actor = NULL;
	m_enabled = FALSE;
	m_isActor = FALSE;
	m_volume = 79;
}

// FUNCTION: LEGO1 0x100116a0
// FUNCTION: BETA10 0x10039647
MxResult Lego3DSound::Create(ma_sound* p_sound, const char* p_name, MxS32 p_volume)
{
	m_volume = p_volume;

	if (MxOmni::IsSound3D()) {
		m_sound = p_sound;

		ma_sound_set_min_distance(m_sound, 15.0f);
		ma_sound_set_max_distance(m_sound, 100.0f);
		ma_sound_set_position(m_sound, 0.0f, 0.0f, 40.0f);
		ma_sound_set_rolloff(m_sound, 10.0f);
	}

	if (m_sound == NULL || p_name == NULL) {
		return SUCCESS;
	}

	if (CharacterManager()->IsActor(p_name)) {
		m_roi = CharacterManager()->GetActorROI(p_name, TRUE);
		m_enabled = m_isActor = TRUE;
	}
	else {
		m_roi = FindROI(p_name);
	}

	if (m_roi == NULL) {
		m_roi = CharacterManager()->CreateAutoROI(NULL, p_name, TRUE);

		if (m_roi != NULL) {
			m_enabled = TRUE;
		}
	}

	if (m_roi == NULL) {
		return SUCCESS;
	}

	if (m_isActor) {
		m_positionROI = m_roi->FindChildROI("head", m_roi);
	}
	else {
		m_positionROI = m_roi;
	}

	if (MxOmni::IsSound3D()) {
		const float* position = m_positionROI->GetWorldPosition();
		ma_sound_set_position(m_sound, position[0], position[1], -position[2]);
	}

	LegoEntity* entity = m_roi->GetEntity();
	if (entity != NULL && entity->IsA("LegoActor") && ((LegoActor*) entity)->GetSoundFrequencyFactor() != 0.0f) {
		m_actor = ((LegoActor*) entity);
	}

	if (m_actor != NULL) {
		m_frequencyFactor = m_actor->GetSoundFrequencyFactor();

		if (m_frequencyFactor != 0.0) {
			ma_sound_set_pitch(p_sound, m_frequencyFactor);
		}
	}

	return SUCCESS;
}

// FUNCTION: LEGO1 0x10011880
void Lego3DSound::Destroy()
{
	m_sound = NULL;

	if (m_enabled && m_roi && CharacterManager()) {
		if (m_isActor) {
			CharacterManager()->ReleaseActor(m_roi);
		}
		else {
			CharacterManager()->ReleaseAutoROI(m_roi);
		}
	}

	Init();
}

// FUNCTION: LEGO1 0x100118e0
// FUNCTION: BETA10 0x10039a2a
MxU32 Lego3DSound::UpdatePosition(ma_sound* p_sound)
{
	MxU32 updated = FALSE;

	if (m_positionROI != NULL) {
		const float* position = m_positionROI->GetWorldPosition();

		ViewROI* pov = VideoManager()->GetViewROI();
		assert(pov);

		const float* povPosition = pov->GetWorldPosition();
		float distance = DISTSQRD3(povPosition, position);

		if (distance > 10000.0f) {
			return FALSE;
		}

		if (m_sound != NULL) {
			ma_sound_set_position(m_sound, position[0], position[1], -position[2]);
		}
		else {
			MxS32 newVolume = m_volume;
			if (distance < 100.0f) {
				newVolume = m_volume;
			}
			else if (distance < 400.0f) {
				newVolume *= 0.4;
			}
			else if (distance < 3600.0f) {
				newVolume *= 0.1;
			}
			else if (distance < 10000.0f) {
				newVolume = 0;
			}

			newVolume = newVolume * SoundManager()->GetVolume() / 100;
			ma_sound_set_volume(p_sound, SoundManager()->GetAttenuation(newVolume));
		}

		updated = TRUE;
	}

	if (m_actor != NULL) {
		if (abs(m_frequencyFactor - m_actor->GetSoundFrequencyFactor()) > 0.0001) {
			m_frequencyFactor = m_actor->GetSoundFrequencyFactor();
			ma_sound_set_pitch(p_sound, m_frequencyFactor);
			updated = TRUE;
		}
	}

	return updated;
}

// FUNCTION: LEGO1 0x10011a60
// FUNCTION: BETA10 0x10039d04
void Lego3DSound::FUN_10011a60(ma_sound* p_sound, const char* p_name)
{
	assert(p_sound);

	if (p_name == NULL) {
		if (m_sound != NULL) {
			ma_sound_set_spatialization_enabled(m_sound, MA_FALSE);
		}
	}
	else {
		if (CharacterManager()->IsActor(p_name)) {
			m_roi = CharacterManager()->GetActorROI(p_name, TRUE);
			m_enabled = m_isActor = TRUE;
		}
		else {
			m_roi = FindROI(p_name);
		}

		if (m_roi == NULL) {
			m_roi = CharacterManager()->CreateAutoROI(NULL, p_name, TRUE);

			if (m_roi != NULL) {
				m_enabled = TRUE;
			}
		}

		if (m_roi == NULL) {
			return;
		}

		if (m_isActor) {
			m_positionROI = m_roi->FindChildROI("head", m_roi);
		}
		else {
			m_positionROI = m_roi;
		}

		if (m_sound != NULL) {
			ma_sound_set_spatialization_enabled(m_sound, MA_TRUE);
			const float* position = m_positionROI->GetWorldPosition();
			ma_sound_set_position(m_sound, position[0], position[1], -position[2]);
		}
		else {
			const float* position = m_positionROI->GetWorldPosition();
			ViewROI* pov = VideoManager()->GetViewROI();

			if (pov != NULL) {
				const float* povPosition = pov->GetWorldPosition();
				float distance = DISTSQRD3(povPosition, position);

				MxS32 newVolume;
				if (distance < 100.0f) {
					newVolume = m_volume;
				}
				else if (distance < 400.0f) {
					newVolume = m_volume * 0.4;
				}
				else if (distance < 3600.0f) {
					newVolume = m_volume * 0.1;
				}
				else {
					newVolume = 0;
				}

				newVolume = newVolume * SoundManager()->GetVolume() / 100;
				ma_sound_set_volume(p_sound, SoundManager()->GetAttenuation(newVolume));
			}
		}

		LegoEntity* entity = m_roi->GetEntity();
		if (entity != NULL && entity->IsA("LegoActor") && ((LegoActor*) entity)->GetSoundFrequencyFactor() != 0.0f) {
			m_actor = ((LegoActor*) entity);
		}

		if (m_actor != NULL) {
			m_frequencyFactor = m_actor->GetSoundFrequencyFactor();

			if (m_frequencyFactor != 0.0) {
				ma_sound_set_pitch(p_sound, m_frequencyFactor);
			}
		}
	}
}

// FUNCTION: LEGO1 0x10011ca0
void Lego3DSound::Reset()
{
	if (m_enabled && m_roi && CharacterManager()) {
		if (m_isActor) {
			CharacterManager()->ReleaseActor(m_roi);
		}
		else {
			CharacterManager()->ReleaseAutoROI(m_roi);
		}
	}

	m_roi = NULL;
	m_positionROI = NULL;
	m_actor = NULL;
}

// FUNCTION: LEGO1 0x10011cf0
// FUNCTION: BETA10 0x10039fe0
MxS32 Lego3DSound::SetDistance(MxS32 p_min, MxS32 p_max)
{
	if (MxOmni::IsSound3D()) {
		if (m_sound == NULL) {
			return -1;
		}

		ma_sound_set_min_distance(m_sound, p_min);
		ma_sound_set_max_distance(m_sound, p_max);
		return 0;
	}

	return 1;
}
