#pragma once

#include "extensions/multiplayer/characteranimator.h"
#include "extensions/multiplayer/customizestate.h"
#include "extensions/multiplayer/protocol.h"
#include "mxgeometry/mxgeometry3d.h"
#include "mxtypes.h"

#include <SDL3/SDL_events.h>
#include <cstdint>

class IslePathActor;
class LegoPathActor;
class LegoROI;
class LegoWorld;

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
	bool IsInMultiPartEmote() const;
	int8_t GetFrozenEmoteId() const;
	void SetDisplayActorIndex(uint8_t p_displayActorIndex);
	uint8_t GetDisplayActorIndex() const { return m_displayActorIndex; }
	LegoROI* GetDisplayROI() const { return m_displayROI; }
	CustomizeState& GetCustomizeState() { return m_customizeState; }

	void ApplyCustomizeChange(uint8_t p_changeType, uint8_t p_partIndex);
	void SetClickAnimObjectId(MxU32 p_clickAnimObjectId) { m_animator.SetClickAnimObjectId(p_clickAnimObjectId); }
	void StopClickAnimation();
	bool IsInVehicle() const { return m_animator.IsInVehicle(); }

	void SetNameBubbleVisible(bool p_visible);

	void OnWorldEnabled(LegoWorld* p_world);
	void OnWorldDisabled(LegoWorld* p_world);

	// Free camera input handling
	void HandleSDLEvent(SDL_Event* p_event);
	bool IsTouchGestureActive() const { return m_touchGestureActive; }

private:
	// Orbit camera helpers
	void ComputeOrbitVectors(Mx3DPointFloat& p_at, Mx3DPointFloat& p_dir, Mx3DPointFloat& p_up) const;
	void ApplyOrbitCamera();
	void ResetOrbitState();
	void ClampPitch();
	void ClampDistance();

	void SetupCamera(LegoPathActor* p_actor);
	void ReinitForCharacter();

	void CreateNameBubble();
	void DestroyNameBubble();

	bool EnsureDisplayROI();
	void CreateDisplayClone();
	void DestroyDisplayClone();
	bool HasDisplayOverride() const { return m_displayROI != nullptr; }

	bool m_enabled;
	bool m_active;
	bool m_pendingWorldTransition; // True between OnWorldEnabled and first Tick; defers camera setup
	LegoROI* m_playerROI;          // Borrowed, not owned

	// Display actor override
	uint8_t m_displayActorIndex;
	LegoROI* m_displayROI; // Owned clone; nullptr = use native ROI
	char m_displayUniqueName[32];
	CustomizeState m_customizeState;

	CharacterAnimator m_animator;

	bool m_showNameBubble;

	// Orbit camera state
	float m_orbitYaw;
	float m_orbitPitch;
	float m_orbitDistance;

	// Touch gesture tracking
	bool m_touchGestureActive = false;
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
