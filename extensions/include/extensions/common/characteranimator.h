#pragma once

#include "extensions/common/animutils.h"
#include "extensions/common/constants.h"
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

struct PropGroup;

// Interface for optional extra animation extensions.
// Consumers provide an implementation; CharacterAnimator delegates to it.
struct IExtraAnimHandler {
	virtual ~IExtraAnimHandler() = default;

	// Returns true if the given extra animation ID is valid.
	virtual bool IsValid(uint8_t p_id) const = 0;

	// Returns true if the extra animation is multi-part (has a secondary/recovery phase).
	// Multi-part animations freeze at the last frame of phase 1 until phase 2 is triggered.
	virtual bool IsMultiPart(uint8_t p_id) const = 0;

	// Get the animation name for a phase (0 = primary, 1 = recovery).
	virtual const char* GetAnimName(uint8_t p_id, int p_phase) const = 0;

	// Get the sound key for a phase (nullptr = no sound).
	virtual const char* GetSoundName(uint8_t p_id, int p_phase) const = 0;

	// Build dynamically-created prop ROIs for an animation.
	virtual void BuildProps(PropGroup& p_group, LegoAnim* p_anim, LegoROI* p_playerROI, uint32_t p_propSuffix) = 0;
};

// Configuration for CharacterAnimator behavior that differs between consumers.
struct CharacterAnimatorConfig {
	// When true, save/restore the parent ROI transform during extra animation playback
	// to prevent scale accumulation (needed for ThirdPersonCameraExt's display clone).
	bool saveExtraAnimTransform;

	// Suffix used for unique naming of prop ROIs.
	uint32_t propSuffix;

	// Optional handler for extra animations. When nullptr, extra animation
	// methods (TriggerExtraAnim, etc.) are no-ops.
	IExtraAnimHandler* extraAnimHandler = nullptr;
};

// A group of dynamically-created prop ROIs for an animation (ride or extra).
struct PropGroup {
	LegoAnim* anim = nullptr;
	LegoROI** roiMap = nullptr;
	MxU32 roiMapSize = 0;
	LegoROI** propROIs = nullptr;
	uint8_t propCount = 0;
};

// Character animation component for walk/idle playback, vehicle ride animations,
// and optional extra animation support via IExtraAnimHandler.
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

	// Extra animation playback (no-op if no handler is set)
	void TriggerExtraAnim(uint8_t p_id, LegoROI* p_roi, bool p_isMoving);
	void SetExtraAnimHandler(IExtraAnimHandler* p_handler) { m_config.extraAnimHandler = p_handler; }

	// Click animation tracking
	void SetClickAnimObjectId(MxU32 p_clickAnimObjectId) { m_clickAnimObjectId = p_clickAnimObjectId; }
	void StopClickAnimation();

	// Stop all sounds that were played against the character ROI.
	// Must be called before the ROI is destroyed to prevent use-after-free
	// in the sound system's 3D position update.
	void StopROISounds();

	// Vehicle ride animation
	void BuildRideAnimation(int8_t p_vehicleType, LegoROI* p_playerROI);
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

	// Extra animation state accessors
	bool IsExtraAnimActive() const { return m_extraAnimActive; }

	// Returns true when an extra animation is blocking movement (multi-part in any phase).
	bool IsExtraAnimBlocking() const
	{
		return m_config.extraAnimHandler &&
			   (m_frozenExtraAnimId >= 0 ||
				(m_extraAnimActive && m_config.extraAnimHandler->IsMultiPart(m_currentExtraAnimId)));
	}
	int8_t GetFrozenExtraAnimId() const { return m_frozenExtraAnimId; }
	void SetFrozenExtraAnimId(int8_t p_id, LegoROI* p_roi);

	// Animation time (needed for vehicle ride tick in ThirdPersonCameraExt)
	float GetAnimTime() const { return m_animTime; }
	void SetAnimTime(float p_time) { m_animTime = p_time; }
	void ResetAnimState();

	static constexpr float ANIM_TIME_SCALE = 2000.0f;
	static constexpr float EXTRA_ANIM_TIME_SCALE = 1000.0f;
	static constexpr float IDLE_DELAY_SECONDS = 2.5f;

private:
	using AnimCache = AnimUtils::AnimCache;

	AnimCache* GetOrBuildAnimCache(LegoROI* p_roi, const char* p_animName);
	void StartExtraAnimPhase(uint8_t p_id, int p_phaseIndex, AnimCache* p_cache, LegoROI* p_roi);
	void ClearFrozenState();
	void ClearPropGroup(PropGroup& p_group);
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

	// Extra animation state
	AnimCache* m_extraAnimCache;
	float m_extraAnimTime;
	float m_extraAnimDuration;
	bool m_extraAnimActive;
	uint8_t m_currentExtraAnimId;
	MxMatrix m_extraAnimParentTransform;

	// Multi-part extra animation frozen state (-1 = not frozen)
	int8_t m_frozenExtraAnimId;
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

	// Extra animation props
	PropGroup m_extraAnimPropGroup;
};

} // namespace Common
} // namespace Extensions
