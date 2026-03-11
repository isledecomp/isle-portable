#include "extensions/multiplayer/characteranimator.h"

#include "3dmanager/lego3dmanager.h"
#include "anim/legoanim.h"
#include "extensions/multiplayer/charactercustomizer.h"
#include "extensions/multiplayer/namebubblerenderer.h"
#include "legoanimpresenter.h"
#include "legocachesoundmanager.h"
#include "legocharactermanager.h"
#include "legosoundmanager.h"
#include "legovideomanager.h"
#include "legoworld.h"
#include "misc.h"
#include "misc/legotree.h"
#include "realtime/realtime.h"
#include "roi/legoroi.h"

#include <SDL3/SDL_stdinc.h>

using namespace Multiplayer;

CharacterAnimator::CharacterAnimator(const CharacterAnimatorConfig& p_config)
	: m_config(p_config), m_walkAnimId(0), m_idleAnimId(0), m_walkAnimCache(nullptr), m_idleAnimCache(nullptr),
	  m_animTime(0.0f), m_idleTime(0.0f), m_idleAnimTime(0.0f), m_wasMoving(false), m_emoteAnimCache(nullptr),
	  m_emoteTime(0.0f), m_emoteDuration(0.0f), m_emoteActive(false), m_currentEmoteId(0), m_frozenEmoteId(-1),
	  m_frozenAnimCache(nullptr), m_frozenAnimDuration(0.0f), m_clickAnimObjectId(0), m_rideAnim(nullptr),
	  m_rideRoiMap(nullptr), m_rideRoiMapSize(0), m_rideVehicleROI(nullptr), m_currentVehicleType(VEHICLE_NONE),
	  m_nameBubble(nullptr)
{
}

CharacterAnimator::~CharacterAnimator()
{
	DestroyNameBubble();
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

	if (m_currentVehicleType != VEHICLE_NONE && m_rideAnim && m_rideRoiMap) {
		walkAnim = m_rideAnim;
		walkRoiMap = m_rideRoiMap;
		walkRoiMapSize = m_rideRoiMapSize;
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

	// Movement interrupts click animations and emotes (but not multi-part emotes)
	if (isMoving && m_frozenEmoteId < 0) {
		StopClickAnimation();
		if (m_emoteActive && !IsMultiPartEmote(m_currentEmoteId)) {
			m_emoteActive = false;
			m_emoteAnimCache = nullptr;
		}
	}

	if (isMoving) {
		// Walking / riding
		if (!walkAnim || !walkRoiMap) {
			return;
		}

		if (p_isMoving) {
			m_animTime += p_deltaTime * 2000.0f;
		}
		float duration = (float) walkAnim->GetDuration();
		if (duration > 0.0f) {
			float timeInCycle = m_animTime - duration * SDL_floorf(m_animTime / duration);

			MxMatrix transform(p_roi->GetLocal2World());
			LegoTreeNode* root = walkAnim->GetRoot();
			for (LegoU32 i = 0; i < root->GetNumChildren(); i++) {
				LegoROI::ApplyAnimationTransformation(root->GetChild(i), transform, (LegoTime) timeInCycle, walkRoiMap);
			}
		}
		m_wasMoving = true;
		m_idleTime = 0.0f;
		m_idleAnimTime = 0.0f;
	}
	else if (m_emoteActive && m_emoteAnimCache && m_emoteAnimCache->anim && m_emoteAnimCache->roiMap) {
		// Emote playback
		m_emoteTime += p_deltaTime * 1000.0f;

		if (m_emoteTime >= m_emoteDuration) {
			if (IsMultiPartEmote(m_currentEmoteId) && m_frozenEmoteId < 0) {
				// Phase 1 completed -> freeze at last frame
				m_frozenEmoteId = (int8_t) m_currentEmoteId;
				m_frozenAnimCache = m_emoteAnimCache;
				m_frozenAnimDuration = m_emoteDuration;
				m_emoteActive = false;
				if (m_config.saveEmoteTransform) {
					m_frozenParentTransform = m_emoteParentTransform;
				}
			}
			else {
				if (IsMultiPartEmote(m_currentEmoteId) && m_frozenEmoteId >= 0) {
					// Phase 2 completed -> unfreeze
					ClearFrozenState();
				}
				// Emote completed -- return to stationary flow
				m_emoteActive = false;
				m_emoteAnimCache = nullptr;
				m_wasMoving = false;
				m_idleTime = 0.0f;
				m_idleAnimTime = 0.0f;
			}
		}
		else {
			MxMatrix transform(m_config.saveEmoteTransform ? m_emoteParentTransform : p_roi->GetLocal2World());

			LegoTreeNode* root = m_emoteAnimCache->anim->GetRoot();
			for (LegoU32 i = 0; i < root->GetNumChildren(); i++) {
				LegoROI::ApplyAnimationTransformation(
					root->GetChild(i),
					transform,
					(LegoTime) m_emoteTime,
					m_emoteAnimCache->roiMap
				);
			}

			// Restore player ROI transform (animation root overwrote it).
			if (m_config.saveEmoteTransform) {
				p_roi->WrappedSetLocal2WorldWithWorldDataUpdate(m_emoteParentTransform);
			}
		}
	}
	else if (m_frozenEmoteId >= 0 && m_frozenAnimCache && m_frozenAnimCache->anim && m_frozenAnimCache->roiMap) {
		// Frozen at last frame of a multi-part emote's phase-1 animation
		MxMatrix transform(m_config.saveEmoteTransform ? m_frozenParentTransform : p_roi->GetLocal2World());

		LegoTreeNode* root = m_frozenAnimCache->anim->GetRoot();
		for (LegoU32 i = 0; i < root->GetNumChildren(); i++) {
			LegoROI::ApplyAnimationTransformation(
				root->GetChild(i),
				transform,
				(LegoTime) m_frozenAnimDuration,
				m_frozenAnimCache->roiMap
			);
		}

		if (m_config.saveEmoteTransform) {
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

		// Hold standing pose for 2.5s, then loop breathing/swaying
		if (m_idleTime >= 2.5f) {
			m_idleAnimTime += p_deltaTime * 1000.0f;
		}

		float duration = (float) m_idleAnimCache->anim->GetDuration();
		if (duration > 0.0f) {
			float timeInCycle = m_idleAnimTime - duration * SDL_floorf(m_idleAnimTime / duration);

			MxMatrix transform(p_roi->GetLocal2World());
			LegoTreeNode* root = m_idleAnimCache->anim->GetRoot();
			for (LegoU32 i = 0; i < root->GetNumChildren(); i++) {
				LegoROI::ApplyAnimationTransformation(
					root->GetChild(i),
					transform,
					(LegoTime) timeInCycle,
					m_idleAnimCache->roiMap
				);
			}
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

void CharacterAnimator::TriggerEmote(uint8_t p_emoteId, LegoROI* p_roi, bool p_isMoving)
{
	if (p_emoteId >= g_emoteAnimCount || !p_roi) {
		return;
	}

	if (IsMultiPartEmote(p_emoteId)) {
		if (m_frozenEmoteId == (int8_t) p_emoteId) {
			// Phase 2: play the recovery animation to unfreeze
			AnimCache* cache = GetOrBuildAnimCache(p_roi, g_emoteEntries[p_emoteId].phases[1].anim);
			if (!cache || !cache->anim) {
				return;
			}

			StopClickAnimation();

			m_currentEmoteId = p_emoteId;
			m_emoteAnimCache = cache;
			m_emoteTime = 0.0f;
			m_emoteDuration = (float) cache->anim->GetDuration();
			m_emoteActive = true;

			const char* sound = g_emoteEntries[p_emoteId].phases[1].sound;
			if (sound) {
				SoundManager()->GetCacheSoundManager()->Play(sound, p_roi->GetName(), FALSE);
			}

			if (m_config.saveEmoteTransform) {
				m_emoteParentTransform = m_frozenParentTransform;
			}
			return;
		}
		else if (m_frozenEmoteId >= 0) {
			// Already frozen in a different emote, ignore
			return;
		}
		// Phase 1: fall through to play the primary animation
	}
	else {
		// One-shot emote: block if moving or frozen in any multi-part emote
		if (p_isMoving || m_frozenEmoteId >= 0) {
			return;
		}
	}

	AnimCache* cache = GetOrBuildAnimCache(p_roi, g_emoteEntries[p_emoteId].phases[0].anim);
	if (!cache || !cache->anim) {
		return;
	}

	StopClickAnimation();

	m_currentEmoteId = p_emoteId;
	m_emoteAnimCache = cache;
	m_emoteTime = 0.0f;
	m_emoteDuration = (float) cache->anim->GetDuration();
	m_emoteActive = true;

	const char* sound = g_emoteEntries[p_emoteId].phases[0].sound;
	if (sound) {
		SoundManager()->GetCacheSoundManager()->Play(sound, p_roi->GetName(), FALSE);
	}

	// Save clean transform to prevent scale accumulation during emote
	if (m_config.saveEmoteTransform) {
		m_emoteParentTransform = p_roi->GetLocal2World();
	}
}

void CharacterAnimator::StopClickAnimation()
{
	if (m_clickAnimObjectId != 0) {
		CharacterCustomizer::StopClickAnimation(m_clickAnimObjectId);
		m_clickAnimObjectId = 0;
	}
}

void CharacterAnimator::BuildRideAnimation(int8_t p_vehicleType, LegoROI* p_playerROI, uint32_t p_vehicleSuffix)
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

	m_rideAnim = static_cast<LegoAnimPresenter*>(presenter)->GetAnimation();
	if (!m_rideAnim) {
		return;
	}

	// Create variant ROI, rename to match animation tree.
	const char* baseName = g_vehicleROINames[p_vehicleType];
	char variantName[48];
	if (p_vehicleSuffix != 0) {
		SDL_snprintf(variantName, sizeof(variantName), "%s_mp_%u", vehicleVariantName, p_vehicleSuffix);
	}
	else {
		SDL_snprintf(variantName, sizeof(variantName), "tp_vehicle");
	}
	m_rideVehicleROI = CharacterManager()->CreateAutoROI(variantName, baseName, FALSE);
	if (m_rideVehicleROI) {
		m_rideVehicleROI->SetName(vehicleVariantName);
	}

	AnimUtils::BuildROIMap(m_rideAnim, p_playerROI, m_rideVehicleROI, m_rideRoiMap, m_rideRoiMapSize);
	m_animTime = 0.0f;
}

void CharacterAnimator::ClearRideAnimation()
{
	if (m_rideRoiMap) {
		delete[] m_rideRoiMap;
		m_rideRoiMap = nullptr;
		m_rideRoiMapSize = 0;
	}
	if (m_rideVehicleROI) {
		VideoManager()->Get3DManager()->Remove(*m_rideVehicleROI);
		CharacterManager()->ReleaseAutoROI(m_rideVehicleROI);
		m_rideVehicleROI = nullptr;
	}
	m_rideAnim = nullptr;
	m_currentVehicleType = VEHICLE_NONE;
}

void CharacterAnimator::InitAnimCaches(LegoROI* p_roi)
{
	m_walkAnimCache = GetOrBuildAnimCache(p_roi, g_walkAnimNames[m_walkAnimId]);
	m_idleAnimCache = GetOrBuildAnimCache(p_roi, g_idleAnimNames[m_idleAnimId]);

	// Rebuild frozen emote cache if the frozen state was set before the ROI was available
	// (e.g. state message arrived before world was ready, or world was re-enabled).
	if (m_frozenEmoteId >= 0 && !m_frozenAnimCache) {
		SetFrozenEmoteId(m_frozenEmoteId, p_roi);
	}
}

void CharacterAnimator::SetFrozenEmoteId(int8_t p_emoteId, LegoROI* p_roi)
{
	if (p_emoteId >= 0 && p_emoteId < g_emoteAnimCount && IsMultiPartEmote((uint8_t) p_emoteId)) {
		AnimCache* cache = p_roi ? GetOrBuildAnimCache(p_roi, g_emoteEntries[p_emoteId].phases[0].anim) : nullptr;
		m_frozenEmoteId = p_emoteId;
		m_frozenAnimCache = cache;
		m_frozenAnimDuration = (cache && cache->anim) ? (float) cache->anim->GetDuration() : 0.0f;
		m_emoteActive = false;
		if (m_config.saveEmoteTransform && p_roi) {
			m_frozenParentTransform = p_roi->GetLocal2World();
		}
	}
	else {
		ClearFrozenState();
	}
}

void CharacterAnimator::ClearFrozenState()
{
	m_frozenEmoteId = -1;
	m_frozenAnimCache = nullptr;
	m_frozenAnimDuration = 0.0f;
}

void CharacterAnimator::ClearAnimCaches()
{
	m_walkAnimCache = nullptr;
	m_idleAnimCache = nullptr;
	m_emoteAnimCache = nullptr;
	m_emoteActive = false;
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
	m_emoteActive = false;
	ClearFrozenState();
}

void CharacterAnimator::ApplyIdleFrame0(LegoROI* p_roi)
{
	if (!p_roi || !m_idleAnimCache || !m_idleAnimCache->anim || !m_idleAnimCache->roiMap) {
		return;
	}

	MxMatrix transform(p_roi->GetLocal2World());
	LegoTreeNode* root = m_idleAnimCache->anim->GetRoot();
	for (LegoU32 i = 0; i < root->GetNumChildren(); i++) {
		LegoROI::ApplyAnimationTransformation(root->GetChild(i), transform, (LegoTime) 0.0f, m_idleAnimCache->roiMap);
	}
}

void CharacterAnimator::CreateNameBubble(const char* p_name)
{
	if (m_nameBubble || !p_name || p_name[0] == '\0') {
		return;
	}

	m_nameBubble = new NameBubbleRenderer();
	m_nameBubble->Create(p_name);
}

void CharacterAnimator::DestroyNameBubble()
{
	if (m_nameBubble) {
		delete m_nameBubble;
		m_nameBubble = nullptr;
	}
}

void CharacterAnimator::SetNameBubbleVisible(bool p_visible)
{
	if (m_nameBubble) {
		m_nameBubble->SetVisible(p_visible);
	}
}

void CharacterAnimator::UpdateNameBubble(LegoROI* p_roi)
{
	if (m_nameBubble) {
		m_nameBubble->Update(p_roi);
	}
}
