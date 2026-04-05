#include "extensions/common/characteranimator.h"

#include "3dmanager/lego3dmanager.h"
#include "anim/legoanim.h"
#include "extensions/common/charactercustomizer.h"
#include "extensions/common/charactertables.h"
#include "legoanimpresenter.h"
#include "legocachesoundmanager.h"
#include "legocachsound.h"
#include "legocharactermanager.h"
#include "legosoundmanager.h"
#include "legovideomanager.h"
#include "legoworld.h"
#include "misc.h"
#include "misc/legotree.h"
#include "realtime/realtime.h"
#include "roi/legoroi.h"

#include <SDL3/SDL_stdinc.h>

using namespace Extensions::Common;

CharacterAnimator::CharacterAnimator(const CharacterAnimatorConfig& p_config)
	: m_config(p_config), m_walkAnimId(0), m_idleAnimId(0), m_walkAnimCache(nullptr), m_idleAnimCache(nullptr),
	  m_animTime(0.0f), m_idleTime(0.0f), m_idleAnimTime(0.0f), m_wasMoving(false), m_extraAnimCache(nullptr),
	  m_extraAnimTime(0.0f), m_extraAnimDuration(0.0f), m_extraAnimActive(false), m_currentExtraAnimId(0),
	  m_frozenExtraAnimId(-1), m_frozenAnimCache(nullptr), m_frozenAnimDuration(0.0f), m_clickAnimObjectId(0),
	  m_currentVehicleType(VEHICLE_NONE)
{
}

CharacterAnimator::~CharacterAnimator()
{
	ClearPropGroup(m_extraAnimPropGroup);
	ClearRideAnimation();
}

CharacterAnimator::AnimCache* CharacterAnimator::GetOrBuildAnimCache(LegoROI* p_roi, const char* p_animName)
{
	return AnimUtils::GetOrBuildAnimCache(m_animCacheMap, p_roi, p_animName);
}

void CharacterAnimator::Tick(float p_deltaTime, LegoROI* p_roi, bool p_isMoving)
{
	if (m_currentVehicleType != VEHICLE_NONE && IsLargeVehicle(m_currentVehicleType)) {
		StopClickAnimation();
		return;
	}

	// Determine the active walk/ride animation and its ROI map
	LegoAnim* walkAnim = nullptr;
	LegoROI** walkRoiMap = nullptr;
	MxU32 walkRoiMapSize = 0;

	if (m_currentVehicleType != VEHICLE_NONE && m_ridePropGroup.anim && m_ridePropGroup.roiMap) {
		walkAnim = m_ridePropGroup.anim;
		walkRoiMap = m_ridePropGroup.roiMap;
		walkRoiMapSize = m_ridePropGroup.roiMapSize;
	}
	else if (m_walkAnimCache && m_walkAnimCache->anim && m_walkAnimCache->roiMap) {
		walkAnim = m_walkAnimCache->anim;
		walkRoiMap = m_walkAnimCache->roiMap;
		walkRoiMapSize = m_walkAnimCache->roiMapSize;
	}

	// Ensure visibility of all mapped ROIs
	if (walkRoiMap) {
		AnimUtils::EnsureROIMapVisibility(walkRoiMap, walkRoiMapSize);
	}
	if (m_idleAnimCache && m_idleAnimCache->roiMap) {
		AnimUtils::EnsureROIMapVisibility(m_idleAnimCache->roiMap, m_idleAnimCache->roiMapSize);
	}

	bool inVehicle = (m_currentVehicleType != VEHICLE_NONE);
	bool isMoving = inVehicle || p_isMoving;

	// Movement interrupts click animations and extra animations (but not frozen multi-part)
	if (isMoving && m_frozenExtraAnimId < 0) {
		StopClickAnimation();
		if (m_extraAnimActive) {
			m_extraAnimActive = false;
			m_extraAnimCache = nullptr;
			ClearPropGroup(m_extraAnimPropGroup);
		}
	}

	if (isMoving) {
		// Walking / riding
		if (!walkAnim || !walkRoiMap) {
			return;
		}

		if (p_isMoving) {
			m_animTime += p_deltaTime * ANIM_TIME_SCALE;
		}
		float duration = (float) walkAnim->GetDuration();
		if (duration > 0.0f) {
			float timeInCycle = m_animTime - duration * SDL_floorf(m_animTime / duration);

			MxMatrix transform(p_roi->GetLocal2World());
			AnimUtils::ApplyTree(walkAnim, transform, (LegoTime) timeInCycle, walkRoiMap);
		}
		m_wasMoving = true;
		m_idleTime = 0.0f;
		m_idleAnimTime = 0.0f;
	}
	else if (m_extraAnimActive && m_extraAnimCache && m_extraAnimCache->anim && m_extraAnimCache->roiMap) {
		// Extra animation playback
		m_extraAnimTime += p_deltaTime * EXTRA_ANIM_TIME_SCALE;

		if (m_extraAnimTime >= m_extraAnimDuration) {
			bool isMultiPart =
				m_config.extraAnimHandler && m_config.extraAnimHandler->IsMultiPart(m_currentExtraAnimId);
			if (isMultiPart && m_frozenExtraAnimId < 0) {
				// Phase 1 completed -> freeze at last frame
				m_frozenExtraAnimId = (int8_t) m_currentExtraAnimId;
				m_frozenAnimCache = m_extraAnimCache;
				m_frozenAnimDuration = m_extraAnimDuration;
				m_extraAnimActive = false;
				if (m_config.saveExtraAnimTransform) {
					m_frozenParentTransform = m_extraAnimParentTransform;
				}
			}
			else {
				if (isMultiPart && m_frozenExtraAnimId >= 0) {
					// Phase 2 completed -> unfreeze
					ClearFrozenState();
				}
				// Extra animation completed -- return to stationary flow
				m_extraAnimActive = false;
				m_extraAnimCache = nullptr;
				ClearPropGroup(m_extraAnimPropGroup);
				m_wasMoving = false;
				m_idleTime = 0.0f;
				m_idleAnimTime = 0.0f;
			}
		}
		else {
			LegoROI** extraRoiMap =
				m_extraAnimPropGroup.roiMap != nullptr ? m_extraAnimPropGroup.roiMap : m_extraAnimCache->roiMap;
			MxMatrix transform(m_config.saveExtraAnimTransform ? m_extraAnimParentTransform : p_roi->GetLocal2World());

			AnimUtils::ApplyTree(m_extraAnimCache->anim, transform, (LegoTime) m_extraAnimTime, extraRoiMap);

			// Restore player ROI transform (animation root overwrote it).
			if (m_config.saveExtraAnimTransform) {
				p_roi->WrappedSetLocal2WorldWithWorldDataUpdate(m_extraAnimParentTransform);
			}
		}
	}
	else if (m_frozenExtraAnimId >= 0 && m_frozenAnimCache && m_frozenAnimCache->anim && m_frozenAnimCache->roiMap) {
		// Frozen at last frame of a multi-part extra animation's phase 1
		MxMatrix transform(m_config.saveExtraAnimTransform ? m_frozenParentTransform : p_roi->GetLocal2World());

		AnimUtils::ApplyTree(
			m_frozenAnimCache->anim,
			transform,
			(LegoTime) m_frozenAnimDuration,
			m_frozenAnimCache->roiMap
		);

		if (m_config.saveExtraAnimTransform) {
			p_roi->WrappedSetLocal2WorldWithWorldDataUpdate(m_frozenParentTransform);
		}
	}
	else if (m_idleAnimCache && m_idleAnimCache->anim && m_idleAnimCache->roiMap) {
		// Idle animation
		if (m_wasMoving) {
			m_wasMoving = false;
			m_idleTime = 0.0f;
			m_idleAnimTime = 0.0f;
		}

		m_idleTime += p_deltaTime;

		// Hold standing pose, then loop breathing/swaying
		if (m_idleTime >= IDLE_DELAY_SECONDS) {
			m_idleAnimTime += p_deltaTime * 1000.0f;
		}

		float duration = (float) m_idleAnimCache->anim->GetDuration();
		if (duration > 0.0f) {
			float timeInCycle = m_idleAnimTime - duration * SDL_floorf(m_idleAnimTime / duration);

			MxMatrix transform(p_roi->GetLocal2World());
			AnimUtils::ApplyTree(m_idleAnimCache->anim, transform, (LegoTime) timeInCycle, m_idleAnimCache->roiMap);
		}
	}
}

void CharacterAnimator::SetWalkAnimId(uint8_t p_walkAnimId, LegoROI* p_roi)
{
	if (p_walkAnimId >= g_walkAnimCount) {
		return;
	}
	if (p_walkAnimId != m_walkAnimId) {
		m_walkAnimId = p_walkAnimId;
		if (p_roi) {
			m_walkAnimCache = GetOrBuildAnimCache(p_roi, g_walkAnimNames[m_walkAnimId]);
		}
	}
}

void CharacterAnimator::SetIdleAnimId(uint8_t p_idleAnimId, LegoROI* p_roi)
{
	if (p_idleAnimId >= g_idleAnimCount) {
		return;
	}
	if (p_idleAnimId != m_idleAnimId) {
		m_idleAnimId = p_idleAnimId;
		if (p_roi) {
			m_idleAnimCache = GetOrBuildAnimCache(p_roi, g_idleAnimNames[m_idleAnimId]);
		}
	}
}

void CharacterAnimator::StartExtraAnimPhase(
	uint8_t p_currentExtraAnimId,
	int p_phaseIndex,
	AnimCache* p_extraAnimCache,
	LegoROI* p_roi
)
{
	StopClickAnimation();
	ClearPropGroup(m_extraAnimPropGroup);

	m_currentExtraAnimId = p_currentExtraAnimId;
	m_extraAnimCache = p_extraAnimCache;
	m_extraAnimTime = 0.0f;
	m_extraAnimDuration = (float) p_extraAnimCache->anim->GetDuration();
	m_extraAnimActive = true;

	if (m_config.extraAnimHandler) {
		m_config.extraAnimHandler->BuildProps(m_extraAnimPropGroup, p_extraAnimCache->anim, p_roi, m_config.propSuffix);
	}

	const char* sound = m_config.extraAnimHandler
							? m_config.extraAnimHandler->GetSoundName(p_currentExtraAnimId, p_phaseIndex)
							: nullptr;
	if (sound) {
		PlayROISound(sound, p_roi);
	}
}

void CharacterAnimator::TriggerExtraAnim(uint8_t p_id, LegoROI* p_roi, bool p_isMoving)
{
	if (!m_config.extraAnimHandler || !m_config.extraAnimHandler->IsValid(p_id)) {
		return;
	}
	if (!p_roi || m_currentVehicleType != VEHICLE_NONE) {
		return;
	}

	bool isMultiPart = m_config.extraAnimHandler->IsMultiPart(p_id);

	if (isMultiPart) {
		if (m_frozenExtraAnimId == (int8_t) p_id) {
			// Phase 2: play the recovery animation to unfreeze
			const char* animName = m_config.extraAnimHandler->GetAnimName(p_id, 1);
			AnimCache* cache = animName ? GetOrBuildAnimCache(p_roi, animName) : nullptr;
			if (!cache || !cache->anim) {
				return;
			}

			StartExtraAnimPhase(p_id, 1, cache, p_roi);

			if (m_config.saveExtraAnimTransform) {
				m_extraAnimParentTransform = m_frozenParentTransform;
			}
			return;
		}
		else if (m_frozenExtraAnimId >= 0) {
			// Already frozen in a different extra animation, ignore
			return;
		}
		// Phase 1: fall through to play the primary animation
	}
	else {
		// One-shot: block if moving or frozen in any multi-part extra animation
		if (p_isMoving || m_frozenExtraAnimId >= 0) {
			return;
		}
	}

	const char* animName = m_config.extraAnimHandler->GetAnimName(p_id, 0);
	AnimCache* cache = animName ? GetOrBuildAnimCache(p_roi, animName) : nullptr;
	if (!cache || !cache->anim) {
		return;
	}

	StartExtraAnimPhase(p_id, 0, cache, p_roi);

	// Save clean transform to prevent scale accumulation during extra animation
	if (m_config.saveExtraAnimTransform) {
		m_extraAnimParentTransform = p_roi->GetLocal2World();
	}
}

void CharacterAnimator::StopClickAnimation()
{
	if (m_clickAnimObjectId != 0) {
		CharacterCustomizer::StopClickAnimation(m_clickAnimObjectId);
		m_clickAnimObjectId = 0;
	}
}

void CharacterAnimator::PlayROISound(const char* p_key, LegoROI* p_roi)
{
	LegoCacheSound* sound = SoundManager()->GetCacheSoundManager()->Play(p_key, p_roi->GetName(), FALSE);
	if (sound) {
		m_ROISounds.push_back(sound);
	}
}

void CharacterAnimator::StopROISounds()
{
	LegoCacheSoundManager* mgr = SoundManager()->GetCacheSoundManager();
	for (LegoCacheSound* sound : m_ROISounds) {
		mgr->Stop(sound);
	}
	m_ROISounds.clear();
}

void CharacterAnimator::BuildRideAnimation(int8_t p_vehicleType, LegoROI* p_playerROI)
{
	if (p_vehicleType < 0 || p_vehicleType >= VEHICLE_COUNT) {
		return;
	}

	const char* rideAnimName = g_rideAnimNames[p_vehicleType];
	const char* vehicleVariantName = g_rideVehicleROINames[p_vehicleType];
	if (!rideAnimName || !vehicleVariantName) {
		return;
	}

	LegoWorld* world = CurrentWorld();
	if (!world) {
		return;
	}

	MxCore* presenter = world->Find("LegoAnimPresenter", rideAnimName);
	if (!presenter) {
		return;
	}

	m_ridePropGroup.anim = static_cast<LegoAnimPresenter*>(presenter)->GetAnimation();
	if (!m_ridePropGroup.anim) {
		return;
	}

	// Create variant ROI, rename to match animation tree.
	const char* baseName = g_vehicleROINames[p_vehicleType];
	char variantName[48];
	if (m_config.propSuffix != 0) {
		SDL_snprintf(variantName, sizeof(variantName), "%s_%u", vehicleVariantName, m_config.propSuffix);
	}
	else {
		SDL_snprintf(variantName, sizeof(variantName), "tp_vehicle");
	}
	LegoROI* vehicleROI = CharacterManager()->CreateAutoROI(variantName, baseName, FALSE);
	if (vehicleROI) {
		vehicleROI->SetName(vehicleVariantName);
		m_ridePropGroup.propROIs = new LegoROI*[1];
		m_ridePropGroup.propROIs[0] = vehicleROI;
		m_ridePropGroup.propCount = 1;
	}

	AnimUtils::BuildROIMap(
		m_ridePropGroup.anim,
		p_playerROI,
		m_ridePropGroup.propROIs,
		m_ridePropGroup.propCount,
		m_ridePropGroup.roiMap,
		m_ridePropGroup.roiMapSize
	);
	m_animTime = 0.0f;
}

void CharacterAnimator::ClearRideAnimation()
{
	ClearPropGroup(m_ridePropGroup);
	m_currentVehicleType = VEHICLE_NONE;
}

void CharacterAnimator::InitAnimCaches(LegoROI* p_roi)
{
	m_walkAnimCache = GetOrBuildAnimCache(p_roi, g_walkAnimNames[m_walkAnimId]);
	m_idleAnimCache = GetOrBuildAnimCache(p_roi, g_idleAnimNames[m_idleAnimId]);

	// Rebuild frozen extra animation cache if the frozen state was set before the ROI
	// was available (e.g. state arrived before world was ready, or world was re-enabled).
	if (m_frozenExtraAnimId >= 0 && !m_frozenAnimCache) {
		SetFrozenExtraAnimId(m_frozenExtraAnimId, p_roi);
	}
}

void CharacterAnimator::SetFrozenExtraAnimId(int8_t p_frozenExtraAnimId, LegoROI* p_roi)
{
	if (m_config.extraAnimHandler && p_frozenExtraAnimId >= 0 &&
		m_config.extraAnimHandler->IsValid((uint8_t) p_frozenExtraAnimId) &&
		m_config.extraAnimHandler->IsMultiPart((uint8_t) p_frozenExtraAnimId)) {
		const char* animName = m_config.extraAnimHandler->GetAnimName((uint8_t) p_frozenExtraAnimId, 0);
		AnimCache* cache = (p_roi && animName) ? GetOrBuildAnimCache(p_roi, animName) : nullptr;
		m_frozenExtraAnimId = p_frozenExtraAnimId;
		m_frozenAnimCache = cache;
		m_frozenAnimDuration = (cache && cache->anim) ? (float) cache->anim->GetDuration() : 0.0f;
		m_extraAnimActive = false;
		if (m_config.saveExtraAnimTransform && p_roi) {
			m_frozenParentTransform = p_roi->GetLocal2World();
		}
	}
	else {
		ClearFrozenState();
	}
}

void CharacterAnimator::ClearFrozenState()
{
	m_frozenExtraAnimId = -1;
	m_frozenAnimCache = nullptr;
	m_frozenAnimDuration = 0.0f;
	ClearPropGroup(m_extraAnimPropGroup);
}

void CharacterAnimator::ClearPropGroup(PropGroup& p_group)
{
	delete[] p_group.roiMap;
	p_group.roiMap = nullptr;
	p_group.roiMapSize = 0;

	for (uint8_t i = 0; i < p_group.propCount; i++) {
		if (p_group.propROIs[i]) {
			VideoManager()->Get3DManager()->Remove(*p_group.propROIs[i]);
			CharacterManager()->ReleaseAutoROI(p_group.propROIs[i]);
		}
	}
	delete[] p_group.propROIs;
	p_group.propROIs = nullptr;
	p_group.propCount = 0;
	p_group.anim = nullptr;
}

void CharacterAnimator::ClearAnimCaches()
{
	m_walkAnimCache = nullptr;
	m_idleAnimCache = nullptr;
	m_extraAnimCache = nullptr;
	m_extraAnimActive = false;
	StopROISounds();
	ClearFrozenState();
}

void CharacterAnimator::ClearAll()
{
	m_animCacheMap.clear();
	ClearAnimCaches();
}

void CharacterAnimator::ResetAnimState()
{
	m_animTime = 0.0f;
	m_idleTime = 0.0f;
	m_idleAnimTime = 0.0f;
	m_wasMoving = false;
	m_extraAnimActive = false;
	ClearFrozenState();
}

void CharacterAnimator::ApplyIdleFrame0(LegoROI* p_roi)
{
	if (!p_roi || !m_idleAnimCache || !m_idleAnimCache->anim || !m_idleAnimCache->roiMap) {
		return;
	}

	MxMatrix transform(p_roi->GetLocal2World());
	AnimUtils::ApplyTree(m_idleAnimCache->anim, transform, (LegoTime) 0.0f, m_idleAnimCache->roiMap);
}
