#pragma once

#include "extensions/multiplayer/animutils.h"
#include "extensions/multiplayer/protocol.h"
#include "mxgeometry/mxmatrix.h"
#include "mxtypes.h"

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
	void SetWalkAnimId(uint8_t p_id);
	void SetIdleAnimId(uint8_t p_id);
	void TriggerEmote(uint8_t p_emoteId);

	void OnWorldEnabled(LegoWorld* p_world);
	void OnWorldDisabled(LegoWorld* p_world);

private:
	using AnimCache = AnimUtils::AnimCache;

	AnimCache* GetOrBuildAnimCache(const char* p_animName);
	void ClearAnimCaches();
	void SetupCamera(LegoPathActor* p_actor);
	void BuildRideAnimation(int8_t p_vehicleType);
	void ClearRideAnimation();
	void ApplyIdleFrame0();
	void ReinitForCharacter();

	bool m_enabled;
	bool m_active;
	bool m_roiUnflipped; // True when Disable() flipped the ROI direction; ReinitForCharacter re-applies
	LegoROI* m_playerROI; // Borrowed, not owned

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

	// Vehicle ride state
	int8_t m_currentVehicleType;
	LegoAnim* m_rideAnim;
	LegoROI** m_rideRoiMap;
	MxU32 m_rideRoiMapSize;
	LegoROI* m_rideVehicleROI;

	std::map<std::string, AnimCache> m_animCacheMap;
};

} // namespace Multiplayer
