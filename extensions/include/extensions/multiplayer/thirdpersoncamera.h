#pragma once

#include "extensions/multiplayer/animutils.h"
#include "extensions/multiplayer/customizestate.h"
#include "extensions/multiplayer/protocol.h"
#include "mxgeometry/mxgeometry3d.h"
#include "mxgeometry/mxmatrix.h"
#include "mxtypes.h"

#include <SDL3/SDL_events.h>
#include <cstdint>
#include <map>
#include <string>

class IslePathActor;
class LegoPathActor;
class LegoROI;
class LegoWorld;
class LegoAnim;

namespace Multiplayer
{

class ThirdPersonCamera {
public:
	ThirdPersonCamera();

	void Enable();
	void Disable();
	bool IsEnabled() const { return m_enabled; }
	bool IsActive() const { return m_active; }

	// Core hooks
	void OnActorEnter(IslePathActor* p_actor);
	void OnActorExit(IslePathActor* p_actor);
	void OnCamAnimEnd(LegoPathActor* p_actor);

	// Called every frame from NetworkManager::Tickle()
	void Tick(float p_deltaTime);

	// Animation selection (forwarded from NetworkManager)
	void SetWalkAnimId(uint8_t p_walkAnimId);
	void SetIdleAnimId(uint8_t p_idleAnimId);
	void TriggerEmote(uint8_t p_emoteId);
	void SetDisplayActorIndex(uint8_t p_displayActorIndex);
	uint8_t GetDisplayActorIndex() const { return m_displayActorIndex; }
	LegoROI* GetDisplayROI() const { return m_displayROI; }
	CustomizeState& GetCustomizeState() { return m_customizeState; }

	void ApplyCustomizeChange(uint8_t p_changeType, uint8_t p_partIndex);
	void SetClickAnimObjectId(MxU32 p_clickAnimObjectId) { m_clickAnimObjectId = p_clickAnimObjectId; }
	void StopClickAnimation();
	bool IsInVehicle() const { return m_currentVehicleType != VEHICLE_NONE; }

	void OnWorldEnabled(LegoWorld* p_world);
	void OnWorldDisabled(LegoWorld* p_world);

	// Free camera input handling
	void HandleSDLEvent(SDL_Event* p_event);

private:
	// Orbit camera helpers
	void ComputeOrbitVectors(Mx3DPointFloat& p_at, Mx3DPointFloat& p_dir, Mx3DPointFloat& p_up) const;
	void ApplyOrbitCamera();
	void ResetOrbitState();
	void ClampPitch();
	void ClampDistance();

	using AnimCache = AnimUtils::AnimCache;

	AnimCache* GetOrBuildAnimCache(const char* p_animName);
	void ClearAnimCaches();
	void SetupCamera(LegoPathActor* p_actor);
	void BuildRideAnimation(int8_t p_vehicleType);
	void ClearRideAnimation();
	void ApplyIdleFrame0();
	void ReinitForCharacter();

	bool EnsureDisplayROI();
	void CreateDisplayClone();
	void DestroyDisplayClone();
	bool HasDisplayOverride() const { return m_displayROI != nullptr; }

	bool m_enabled;
	bool m_active;
	bool m_roiUnflipped; // True when Disable() flipped the ROI direction; ReinitForCharacter re-applies
	LegoROI* m_playerROI; // Borrowed, not owned

	// Display actor override
	uint8_t m_displayActorIndex;
	LegoROI* m_displayROI;           // Owned clone; nullptr = use native ROI
	char m_displayUniqueName[32];
	CustomizeState m_customizeState;

	// Walk/idle state (same pattern as RemotePlayer)
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
	MxMatrix m_emoteParentTransform;

	// Click animation tracking (0 = none)
	MxU32 m_clickAnimObjectId;

	// Vehicle ride state
	int8_t m_currentVehicleType;
	LegoAnim* m_rideAnim;
	LegoROI** m_rideRoiMap;
	MxU32 m_rideRoiMapSize;
	LegoROI* m_rideVehicleROI;

	std::map<std::string, AnimCache> m_animCacheMap;

	// Orbit camera state
	float m_orbitYaw;
	float m_orbitPitch;
	float m_orbitDistance;

	// Touch gesture tracking
	struct TouchState {
		SDL_FingerID id[2];
		float x[2], y[2];
		int count;
		float initialPinchDist;
	} m_touch;

	static constexpr float DEFAULT_ORBIT_YAW = 0.0f;
	static constexpr float DEFAULT_ORBIT_PITCH = 0.3f;
	static constexpr float DEFAULT_ORBIT_DISTANCE = 3.5f;
	static constexpr float ORBIT_TARGET_HEIGHT = 1.5f;
	static constexpr float MIN_PITCH = 0.05f;
	static constexpr float MAX_PITCH = 1.4f;
	static constexpr float MIN_DISTANCE = 1.5f;
	static constexpr float MAX_DISTANCE = 15.0f;
};

} // namespace Multiplayer
