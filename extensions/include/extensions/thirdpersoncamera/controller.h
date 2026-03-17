#pragma once

#include "extensions/common/characteranimator.h"
#include "extensions/thirdpersoncamera/displayactor.h"
#include "extensions/thirdpersoncamera/inputhandler.h"
#include "extensions/thirdpersoncamera/orbitcamera.h"
#include "mxtypes.h"

#include <SDL3/SDL_events.h>
#include <cstdint>

class IslePathActor;
class LegoNavController;
class LegoPathActor;
class LegoROI;
class LegoWorld;
class Vector3;

namespace Extensions
{
namespace ThirdPersonCamera
{

class Controller {
public:
	Controller();

	void Enable();
	void Disable(bool p_preserveTouch = false);
	bool IsEnabled() const { return m_enabled; }
	bool IsActive() const { return m_active; }

	void OnActorEnter(IslePathActor* p_actor);
	void OnActorExit(IslePathActor* p_actor);
	void OnCamAnimEnd(LegoPathActor* p_actor);

	void Tick(float p_deltaTime);

	void SetWalkAnimId(uint8_t p_walkAnimId);
	uint8_t GetWalkAnimId() const { return m_animator.GetWalkAnimId(); }
	void SetIdleAnimId(uint8_t p_idleAnimId);
	uint8_t GetIdleAnimId() const { return m_animator.GetIdleAnimId(); }
	void TriggerEmote(uint8_t p_emoteId);
	bool IsInMultiPartEmote() const;
	int8_t GetFrozenEmoteId() const;

	void SetDisplayActorIndex(uint8_t p_displayActorIndex) { m_display.SetDisplayActorIndex(p_displayActorIndex); }
	uint8_t GetDisplayActorIndex() const { return m_display.GetDisplayActorIndex(); }
	LegoROI* GetDisplayROI() const { return m_display.GetDisplayROI(); }
	Common::CustomizeState& GetCustomizeState() { return m_display.GetCustomizeState(); }

	void ApplyCustomizeChange(uint8_t p_changeType, uint8_t p_partIndex)
	{
		m_display.ApplyCustomizeChange(p_changeType, p_partIndex);
	}
	void SetClickAnimObjectId(MxU32 p_clickAnimObjectId) { m_animator.SetClickAnimObjectId(p_clickAnimObjectId); }
	void StopClickAnimation();
	bool IsInVehicle() const { return m_animator.IsInVehicle(); }

	void OnWorldEnabled(LegoWorld* p_world);
	void OnWorldDisabled(LegoWorld* p_world);

	MxBool HandleCameraRelativeMovement(
		LegoNavController* p_nav,
		const Vector3& p_curPos,
		const Vector3& p_curDir,
		Vector3& p_newPos,
		Vector3& p_newDir,
		float p_deltaTime
	);

	void HandleSDLEventImpl(SDL_Event* p_event);

	bool ConsumeAutoDisable() { return m_input.ConsumeAutoDisable(); }
	bool ConsumeAutoEnable() { return m_input.ConsumeAutoEnable(); }

	bool IsLeftButtonHeld() const { return m_input.IsLeftButtonHeld(); }
	bool IsLmbForwardEngaged() const { return m_lmbForwardEngaged; }
	void SetLmbForwardEngaged(bool p_engaged) { m_lmbForwardEngaged = p_engaged; }

	MxBool HandleFirstPersonForward(
		LegoNavController* p_nav,
		const Vector3& p_curPos,
		const Vector3& p_curDir,
		Vector3& p_newPos,
		Vector3& p_newDir,
		float p_deltaTime
	);

	float GetOrbitDistance() const { return m_orbit.GetOrbitDistance(); }
	void SetOrbitDistance(float p_distance) { m_orbit.SetOrbitDistance(p_distance); }
	void ResetTouchState() { m_input.ResetTouchState(); }
	void SuppressGestures() { m_input.SuppressGestures(); }

	bool TryClaimFinger(const SDL_TouchFingerEvent& event) { return m_input.TryClaimFinger(event, m_active); }
	bool TryReleaseFinger(SDL_FingerID id) { return m_input.TryReleaseFinger(id); }
	bool IsFingerTracked(SDL_FingerID id) const { return m_input.IsFingerTracked(id); }
	int GetTouchCount() const { return m_input.GetTouchCount(); }
	SDL_FingerID GetFingerID(int idx) const { return m_input.GetFingerID(idx); }

	void FreezeDisplayActor() { m_display.FreezeDisplayActor(); }
	void UnfreezeDisplayActor() { m_display.UnfreezeDisplayActor(); }
	bool IsDisplayActorFrozen() const { return m_display.IsDisplayActorFrozen(); }

	LegoROI* GetPlayerROI() const { return m_playerROI; }

	static constexpr float CAMERA_ZONE_X = InputHandler::CAMERA_ZONE_X;
	static constexpr float MIN_DISTANCE = OrbitCamera::MIN_DISTANCE;

private:
	void Deactivate();
	void ReinitForCharacter();

	OrbitCamera m_orbit;
	InputHandler m_input;
	DisplayActor m_display;
	Common::CharacterAnimator m_animator;

	bool m_enabled;
	bool m_active;
	bool m_pendingWorldTransition;
	bool m_lmbForwardEngaged;
	LegoROI* m_playerROI;
};

} // namespace ThirdPersonCamera
} // namespace Extensions
