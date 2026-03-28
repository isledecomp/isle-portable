#pragma once

#include "extensions/common/animutils.h"
#include "extensions/common/charactertables.h"
#include "mxgeometry/mxmatrix.h"
#include "mxtypes.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

class LegoCacheSound;
class LegoROI;
class LegoAnim;

namespace Extensions
{
namespace Common
{

// Configuration for CharacterAnimator behavior that differs between consumers.
struct CharacterAnimatorConfig {
	// When true, save/restore the parent ROI transform during emote playback
	// to prevent scale accumulation (needed for ThirdPersonCameraExt's display clone).
	bool saveEmoteTransform;

	// Suffix used for unique naming of prop ROIs.
	// Remote players use m_peerId, local player uses 0.
	uint32_t propSuffix;
};

// A group of dynamically-created prop ROIs for an animation (ride or emote).
struct PropGroup {
	LegoAnim* anim = nullptr;
	LegoROI** roiMap = nullptr;
	MxU32 roiMapSize = 0;
	LegoROI** propROIs = nullptr;
	uint8_t propCount = 0;
};

// Unified character animation component used by both RemotePlayer and ThirdPersonCameraExt.
// Handles walk/idle/emote animation playback, vehicle ride animations, and click animation
// tracking.
class CharacterAnimator {
public:
	explicit CharacterAnimator(const CharacterAnimatorConfig& p_config);
	~CharacterAnimator();

	// Core animation tick. Call each frame with the character's ROI and movement state.
	void Tick(float p_deltaTime, LegoROI* p_roi, bool p_isMoving);

	// Walk/idle animation selection
	void SetWalkAnimId(uint8_t p_walkAnimId, LegoROI* p_roi);
	void SetIdleAnimId(uint8_t p_idleAnimId, LegoROI* p_roi);
	uint8_t GetWalkAnimId() const { return m_walkAnimId; }
	uint8_t GetIdleAnimId() const { return m_idleAnimId; }

	// Emote playback
	void TriggerEmote(uint8_t p_emoteId, LegoROI* p_roi, bool p_isMoving);

	// Click animation tracking
	void SetClickAnimObjectId(MxU32 p_clickAnimObjectId) { m_clickAnimObjectId = p_clickAnimObjectId; }
	void StopClickAnimation();

	// Stop all sounds that were played against the character ROI.
	// Must be called before the ROI is destroyed to prevent use-after-free
	// in the sound system's 3D position update.
	void StopROISounds();

	// Vehicle ride animation
	void BuildRideAnimation(int8_t p_vehicleType, LegoROI* p_playerROI, uint32_t p_vehicleSuffix);
	void ClearRideAnimation();
	int8_t GetCurrentVehicleType() const { return m_currentVehicleType; }
	void SetCurrentVehicleType(int8_t p_vehicleType) { m_currentVehicleType = p_vehicleType; }
	bool IsInVehicle() const { return m_currentVehicleType != VEHICLE_NONE; }
	LegoROI* GetRideVehicleROI() const { return m_ridePropGroup.propCount > 0 ? m_ridePropGroup.propROIs[0] : nullptr; }
	LegoAnim* GetRideAnim() const { return m_ridePropGroup.anim; }
	LegoROI** GetRideRoiMap() const { return m_ridePropGroup.roiMap; }
	MxU32 GetRideRoiMapSize() const { return m_ridePropGroup.roiMapSize; }

	// Animation cache management
	void InitAnimCaches(LegoROI* p_roi);
	void ClearAnimCaches();
	void ClearAll();
	void ApplyIdleFrame0(LegoROI* p_roi);

	// Emote state accessors
	bool IsEmoteActive() const { return m_emoteActive; }

	// Multi-part emote state. Returns true when the player is in any phase of a multi-part
	// emote (playing phase 1, frozen at last frame, or playing phase 2). Movement is blocked.
	bool IsInMultiPartEmote() const
	{
		return m_frozenEmoteId >= 0 || (m_emoteActive && IsMultiPartEmote(m_currentEmoteId));
	}
	int8_t GetFrozenEmoteId() const { return m_frozenEmoteId; }
	void SetFrozenEmoteId(int8_t p_emoteId, LegoROI* p_roi);

	// Animation time (needed for vehicle ride tick in ThirdPersonCameraExt)
	float GetAnimTime() const { return m_animTime; }
	void SetAnimTime(float p_time) { m_animTime = p_time; }
	void ResetAnimState();

	static constexpr float ANIM_TIME_SCALE = 2000.0f;
	static constexpr float EMOTE_TIME_SCALE = 1000.0f;
	static constexpr float IDLE_DELAY_SECONDS = 2.5f;

private:
	using AnimCache = AnimUtils::AnimCache;

	AnimCache* GetOrBuildAnimCache(LegoROI* p_roi, const char* p_animName);
	void StartEmotePhase(uint8_t p_emoteId, int p_phaseIndex, AnimCache* p_cache, LegoROI* p_roi);
	void ClearFrozenState();
	void ClearPropGroup(PropGroup& p_group);
	void BuildEmoteProps(PropGroup& p_group, LegoAnim* p_anim, LegoROI* p_playerROI);
	void PlayROISound(const char* p_key, LegoROI* p_roi);

	CharacterAnimatorConfig m_config;

	// Walk/idle state
	uint8_t m_walkAnimId;
	uint8_t m_idleAnimId;
	AnimCache* m_walkAnimCache;
	AnimCache* m_idleAnimCache;
	float m_animTime;
	float m_idleTime;
	float m_idleAnimTime;
	bool m_wasMoving;

	// Emote state
	AnimCache* m_emoteAnimCache;
	float m_emoteTime;
	float m_emoteDuration;
	bool m_emoteActive;
	uint8_t m_currentEmoteId;
	MxMatrix m_emoteParentTransform;

	// Multi-part emote frozen state (-1 = not frozen)
	int8_t m_frozenEmoteId;
	AnimCache* m_frozenAnimCache;
	float m_frozenAnimDuration;
	MxMatrix m_frozenParentTransform;

	// Click animation tracking (0 = none)
	MxU32 m_clickAnimObjectId;

	// Sounds played against the character ROI, tracked so they can be
	// stopped before the ROI is destroyed.
	std::vector<LegoCacheSound*> m_ROISounds;

	// ROI map cache: animation name -> cached ROI map
	std::map<std::string, AnimCache> m_animCacheMap;

	// Ride animation (vehicle-specific)
	PropGroup m_ridePropGroup;
	int8_t m_currentVehicleType;

	// Emote prop animation (dynamically-created props for emotes like Toss)
	PropGroup m_emotePropGroup;
};

} // namespace Common
} // namespace Extensions
