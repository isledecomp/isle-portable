#include "extensions/multiplayer/thirdpersoncamera.h"

#include "3dmanager/lego3dmanager.h"
#include "anim/legoanim.h"
#include "extensions/multiplayer/charactercloner.h"
#include "extensions/multiplayer/charactercustomizer.h"
#include "islepathactor.h"
#include "legocameracontroller.h"
#include "legocharactermanager.h"
#include "legovideomanager.h"
#include "legoworld.h"
#include "misc.h"
#include "misc/legotree.h"
#include "mxgeometry/mxgeometry3d.h"
#include "mxgeometry/mxmatrix.h"
#include "realtime/realtime.h"
#include "roi/legoroi.h"

#include <SDL3/SDL_stdinc.h>

using namespace Multiplayer;

// Flip the ROI's z-axis direction in place (same operation as IslePathActor::TurnAround).
static void FlipROIDirection(LegoROI* p_roi)
{
	MxMatrix transform(p_roi->GetLocal2World());
	Vector3 right(transform[0]);
	Vector3 up(transform[1]);
	Vector3 direction(transform[2]);
	direction *= -1.0f;
	right.EqualsCross(up, direction);
	p_roi->SetLocal2World(transform);
	p_roi->WrappedUpdateWorldData();
}

ThirdPersonCamera::ThirdPersonCamera()
	: m_enabled(false), m_active(false), m_roiUnflipped(false), m_playerROI(nullptr),
	  m_displayActorIndex(DISPLAY_ACTOR_NONE), m_displayROI(nullptr),
	  m_animator(CharacterAnimatorConfig{/*.saveEmoteTransform=*/true}), m_showNameBubble(true),
	  m_orbitYaw(DEFAULT_ORBIT_YAW), m_orbitPitch(DEFAULT_ORBIT_PITCH), m_orbitDistance(DEFAULT_ORBIT_DISTANCE),
	  m_touch{}
{
	SDL_memset(m_displayUniqueName, 0, sizeof(m_displayUniqueName));
}

void ThirdPersonCamera::Enable()
{
	m_enabled = true;
	ReinitForCharacter();
}

void ThirdPersonCamera::Disable()
{
	m_enabled = false;

	if (m_active && m_playerROI) {
		LegoPathActor* userActor = UserActor();
		LegoWorld* world = CurrentWorld();

		// Undo TurnAround so the ROI z-axis points in the visual forward
		// direction.  This keeps the 1st-person camera facing the same way
		// as the 3rd-person camera, and ensures the network direction stays
		// consistent (no 180-degree flip for others).
		// Flip the native ROI (not the display clone) since TransformPointOfView
		// uses it for the 1st-person camera.
		LegoROI* turnAroundROI = userActor ? userActor->GetROI() : nullptr;

		if (turnAroundROI) {
			FlipROIDirection(turnAroundROI);
			m_roiUnflipped = true;
		}

		m_playerROI->SetVisibility(FALSE);
		VideoManager()->Get3DManager()->Remove(*m_playerROI);

		// Restore vanilla 1st-person camera (eye-height offset, same as ResetWorldTransform).
		if (userActor && world && world->GetCameraController()) {
			world->GetCameraController()->SetWorldTransform(
				Mx3DPointFloat(0.0F, 1.25F, 0.0F),
				Mx3DPointFloat(0.0F, 0.0F, 1.0F),
				Mx3DPointFloat(0.0F, 1.0F, 0.0F)
			);
			userActor->TransformPointOfView();
		}
	}

	m_active = false;
	DestroyNameBubble();
	DestroyDisplayClone();
	m_animator.ClearRideAnimation();
	m_animator.ClearAll();

	ResetOrbitState();
}

void ThirdPersonCamera::OnActorEnter(IslePathActor* p_actor)
{
	LegoPathActor* userActor = UserActor();
	if (static_cast<LegoPathActor*>(p_actor) != userActor) {
		return;
	}

	// Always track vehicle type so OnActorExit can handle exits
	// even if Enable() was called after entering the vehicle.
	m_animator.SetCurrentVehicleType(DetectVehicleType(userActor));

	// Enter() calls TurnAround(), so any previous undo is superseded.
	m_roiUnflipped = false;

	if (!m_enabled) {
		return;
	}

	LegoROI* newROI = userActor->GetROI();
	if (!newROI) {
		return;
	}

	if (m_animator.GetCurrentVehicleType() != VEHICLE_NONE) {
		// Large vehicles and helicopter: stay first-person.
		if (IsLargeVehicle(m_animator.GetCurrentVehicleType()) ||
			m_animator.GetCurrentVehicleType() == VEHICLE_HELICOPTER) {
			// Hide walking character ROI (Enter doesn't call Exit on it).
			if (m_playerROI) {
				m_playerROI->SetVisibility(FALSE);
				VideoManager()->Get3DManager()->Remove(*m_playerROI);
			}
			DestroyNameBubble();
			m_active = false;
			return;
		}

		// Small vehicle: need the character ROI for ride animations.
		if (!m_playerROI) {
			return;
		}

		// Undo Enter()'s TurnAround.  Vehicles are placed with ROI z opposite
		// to their visual forward (mesh faces -z = forward).  Enter()'s
		// TurnAround flips ROI z to match the visual forward, which breaks
		// the backward-z convention the 3rd-person camera relies on.
		p_actor->TurnAround();

		m_active = true;
		SetupCamera(userActor);
		m_animator.BuildRideAnimation(m_animator.GetCurrentVehicleType(), m_playerROI, 0);
		CreateNameBubble();
		return;
	}

	// Non-vehicle (walking character) entry — Enter() already called TurnAround.
	newROI->SetVisibility(FALSE);
	if (!EnsureDisplayROI()) {
		return;
	}
	m_roiUnflipped = false;
	m_active = true;

	m_playerROI->SetVisibility(TRUE);

	// Re-add ROI so it renders in third-person (SpawnPlayer removes it).
	VideoManager()->Get3DManager()->Remove(*m_playerROI);
	VideoManager()->Get3DManager()->Add(*m_playerROI);

	// Build animation caches and reset state
	m_animator.InitAnimCaches(m_playerROI);
	m_animator.ResetAnimState();

	m_animator.ApplyIdleFrame0(m_playerROI);

	SetupCamera(userActor);
	CreateNameBubble();
}

void ThirdPersonCamera::OnActorExit(IslePathActor* p_actor)
{
	if (!m_enabled) {
		return;
	}

	// For vehicle exit, p_actor is the vehicle, not UserActor —
	// check m_currentVehicleType instead.
	if (m_animator.GetCurrentVehicleType() != VEHICLE_NONE) {
		// When 3rd-person camera is active, movement inversion causes the
		// vehicle to physically drive opposite to vanilla.  CalculateTransform
		// re-inverts to keep the ROI z backward.  Exit()'s TurnAround restores
		// the vanilla convention, but that's wrong for the visual driving
		// direction.  Flip once more so the parked vehicle faces the way it
		// was visually driven.
		if (m_active) {
			p_actor->TurnAround();
		}

		// Exiting a vehicle: reinitialize for the walking character.
		m_animator.ClearRideAnimation();
		m_animator.ClearAll();
		ReinitForCharacter();
	}
	else if (m_active && static_cast<LegoPathActor*>(p_actor) == UserActor()) {
		// Exiting on foot: full teardown.
		DestroyNameBubble();
		if (m_playerROI) {
			m_playerROI->SetVisibility(FALSE);
			VideoManager()->Get3DManager()->Remove(*m_playerROI);
		}
		m_animator.ClearRideAnimation();
		m_animator.ClearAll();
		m_playerROI = nullptr;
		m_active = false;
	}
}

void ThirdPersonCamera::OnCamAnimEnd(LegoPathActor* p_actor)
{
	if (!m_active) {
		return;
	}

	// FUN_1004b6d0's PlaceActor set the ROI with standard direction
	// (z = visual forward). The 3rd person camera needs backward-z.
	// Flip the ROI direction, then re-setup the camera.
	// Flip the native ROI (not the display clone) since Tick() syncs the
	// clone's transform from it.
	LegoROI* roi = p_actor->GetROI();
	if (roi) {
		FlipROIDirection(roi);
	}

	SetupCamera(p_actor);
}

void ThirdPersonCamera::Tick(float p_deltaTime)
{
	if (!m_active) {
		return;
	}

	if (!m_playerROI) {
		return;
	}

	// Update orbit camera position each frame so it tracks the player
	ApplyOrbitCamera();

	m_animator.UpdateNameBubble(m_playerROI);

	// Small vehicle with ride animation
	if (m_animator.GetCurrentVehicleType() != VEHICLE_NONE) {
		m_animator.StopClickAnimation();
		if (m_animator.GetRideAnim() && m_animator.GetRideRoiMap()) {
			LegoPathActor* actor = UserActor();
			if (!actor || !actor->GetROI()) {
				return;
			}

			// Force visibility of ride ROI map entries
			AnimUtils::EnsureROIMapVisibility(m_animator.GetRideRoiMap(), m_animator.GetRideRoiMapSize());

			// Only advance animation time when actually moving
			float speed = actor->GetWorldSpeed();
			if (SDL_fabsf(speed) > 0.01f) {
				m_animator.SetAnimTime(m_animator.GetAnimTime() + p_deltaTime * 2000.0f);
			}

			// Use vehicle actor's transform as base.
			MxMatrix transform(actor->GetROI()->GetLocal2World());

			// Position character ROI at the vehicle for bone rendering.
			m_playerROI->WrappedSetLocal2WorldWithWorldDataUpdate(transform);
			m_playerROI->SetVisibility(TRUE);

			float duration = (float) m_animator.GetRideAnim()->GetDuration();
			if (duration > 0.0f) {
				float timeInCycle =
					m_animator.GetAnimTime() - duration * SDL_floorf(m_animator.GetAnimTime() / duration);

				LegoTreeNode* root = m_animator.GetRideAnim()->GetRoot();
				for (LegoU32 i = 0; i < root->GetNumChildren(); i++) {
					LegoROI::ApplyAnimationTransformation(
						root->GetChild(i),
						transform,
						(LegoTime) timeInCycle,
						m_animator.GetRideRoiMap()
					);
				}
			}
		}
		return;
	}

	LegoPathActor* userActor = UserActor();
	if (!userActor) {
		return;
	}

	// Sync display clone position from native ROI
	if (m_displayROI && m_displayROI == m_playerROI) {
		LegoROI* nativeROI = userActor->GetROI();
		if (nativeROI) {
			MxMatrix mat(nativeROI->GetLocal2World());
			m_displayROI->WrappedSetLocal2WorldWithWorldDataUpdate(mat);
			VideoManager()->Get3DManager()->Moved(*m_displayROI);
		}
	}

	float speed = userActor->GetWorldSpeed();
	bool isMoving = SDL_fabsf(speed) > 0.01f;

	m_animator.Tick(p_deltaTime, m_playerROI, isMoving);
}

void ThirdPersonCamera::SetWalkAnimId(uint8_t p_walkAnimId)
{
	m_animator.SetWalkAnimId(p_walkAnimId, m_active ? m_playerROI : nullptr);
}

void ThirdPersonCamera::SetIdleAnimId(uint8_t p_idleAnimId)
{
	m_animator.SetIdleAnimId(p_idleAnimId, m_active ? m_playerROI : nullptr);
}

void ThirdPersonCamera::TriggerEmote(uint8_t p_emoteId)
{
	if (!m_active) {
		return;
	}

	LegoPathActor* userActor = UserActor();
	if (!userActor) {
		return;
	}

	m_animator.TriggerEmote(p_emoteId, m_playerROI, SDL_fabsf(userActor->GetWorldSpeed()) > 0.01f);
}

void ThirdPersonCamera::ApplyCustomizeChange(uint8_t p_changeType, uint8_t p_partIndex)
{
	uint8_t actorInfoIndex = CharacterCustomizer::ResolveActorInfoIndex(m_displayActorIndex);

	CharacterCustomizer::ApplyChange(m_displayROI, actorInfoIndex, m_customizeState, p_changeType, p_partIndex);
}

void ThirdPersonCamera::StopClickAnimation()
{
	m_animator.StopClickAnimation();
}

void ThirdPersonCamera::OnWorldEnabled(LegoWorld* p_world)
{
	if (!m_enabled || !p_world) {
		return;
	}

	// Animation presenters may have been recreated.
	m_animator.ClearAll();

	ReinitForCharacter();
}

void ThirdPersonCamera::OnWorldDisabled(LegoWorld* p_world)
{
	if (!p_world) {
		return;
	}

	m_active = false;
	m_roiUnflipped = false;
	m_playerROI = nullptr;
	DestroyNameBubble();
	DestroyDisplayClone();
	m_animator.ClearRideAnimation();
	m_animator.ClearAll();
}

void ThirdPersonCamera::SetupCamera(LegoPathActor* p_actor)
{
	LegoWorld* world = CurrentWorld();
	if (!world || !world->GetCameraController()) {
		return;
	}

	Mx3DPointFloat at, dir, up;
	ComputeOrbitVectors(at, dir, up);
	world->GetCameraController()->SetWorldTransform(at, dir, up);
	p_actor->TransformPointOfView();
}

void ThirdPersonCamera::SetDisplayActorIndex(uint8_t p_displayActorIndex)
{
	if (m_displayActorIndex != p_displayActorIndex) {
		m_customizeState.InitFromActorInfo(p_displayActorIndex);
	}
	m_displayActorIndex = p_displayActorIndex;
}

bool ThirdPersonCamera::EnsureDisplayROI()
{
	if (!IsValidDisplayActorIndex(m_displayActorIndex)) {
		return false;
	}
	if (!m_displayROI) {
		CreateDisplayClone();
	}
	if (!m_displayROI) {
		return false;
	}
	m_playerROI = m_displayROI;
	return true;
}

void ThirdPersonCamera::CreateDisplayClone()
{
	if (!IsValidDisplayActorIndex(m_displayActorIndex)) {
		return;
	}
	LegoCharacterManager* charMgr = CharacterManager();
	const char* actorName = charMgr->GetActorName(m_displayActorIndex);
	if (!actorName) {
		return;
	}
	SDL_snprintf(m_displayUniqueName, sizeof(m_displayUniqueName), "tp_display");
	m_displayROI = CharacterCloner::Clone(charMgr, m_displayUniqueName, actorName);

	if (m_displayROI) {
		// Reapply existing customize state to the new clone (preserves state across world transitions).
		// The state is only reset to defaults when the display actor index changes (SetDisplayActorIndex).
		CharacterCustomizer::ApplyFullState(m_displayROI, m_displayActorIndex, m_customizeState);
	}
}

void ThirdPersonCamera::DestroyDisplayClone()
{
	m_animator.StopClickAnimation();
	if (m_displayROI) {
		if (m_playerROI == m_displayROI) {
			m_playerROI = nullptr;
		}
		VideoManager()->Get3DManager()->Remove(*m_displayROI);
		CharacterManager()->ReleaseActor(m_displayUniqueName);
		m_displayROI = nullptr;
	}
}

void ThirdPersonCamera::CreateNameBubble()
{
	char name[8] = {};
	EncodeUsername(name);

	if (name[0] == '\0') {
		return;
	}

	m_animator.CreateNameBubble(name);

	if (!m_showNameBubble) {
		m_animator.SetNameBubbleVisible(false);
	}
}

void ThirdPersonCamera::DestroyNameBubble()
{
	m_animator.DestroyNameBubble();
}

void ThirdPersonCamera::SetNameBubbleVisible(bool p_visible)
{
	m_showNameBubble = p_visible;
	m_animator.SetNameBubbleVisible(p_visible);
}

void ThirdPersonCamera::ComputeOrbitVectors(Mx3DPointFloat& p_at, Mx3DPointFloat& p_dir, Mx3DPointFloat& p_up) const
{
	// Convert spherical coordinates to camera offset in entity-local space.
	// Entity local Z+ is "behind" (after TurnAround), which is where yaw=0 points.
	float cosP = SDL_cosf(m_orbitPitch);
	float sinP = SDL_sinf(m_orbitPitch);
	float sinY = SDL_sinf(m_orbitYaw);
	float cosY = SDL_cosf(m_orbitYaw);

	p_at = Mx3DPointFloat(
		m_orbitDistance * sinY * cosP,
		ORBIT_TARGET_HEIGHT + m_orbitDistance * sinP,
		m_orbitDistance * cosY * cosP
	);

	// Direction points from camera toward the pivot. Since the camera sits on
	// a sphere of radius m_orbitDistance, the unit direction is just the
	// negated spherical unit vector.
	p_dir = Mx3DPointFloat(-sinY * cosP, -sinP, -cosY * cosP);

	p_up = Mx3DPointFloat(0.0f, 1.0f, 0.0f);
}

void ThirdPersonCamera::ApplyOrbitCamera()
{
	LegoPathActor* actor = UserActor();
	LegoWorld* world = CurrentWorld();
	if (!actor || !world || !world->GetCameraController()) {
		return;
	}

	Mx3DPointFloat at, dir, up;
	ComputeOrbitVectors(at, dir, up);
	world->GetCameraController()->SetWorldTransform(at, dir, up);
	actor->TransformPointOfView();
}

void ThirdPersonCamera::ResetOrbitState()
{
	m_orbitYaw = DEFAULT_ORBIT_YAW;
	m_orbitPitch = DEFAULT_ORBIT_PITCH;
	m_orbitDistance = DEFAULT_ORBIT_DISTANCE;
	m_touch = {};
}

void ThirdPersonCamera::ClampPitch()
{
	if (m_orbitPitch < MIN_PITCH) {
		m_orbitPitch = MIN_PITCH;
	}
	if (m_orbitPitch > MAX_PITCH) {
		m_orbitPitch = MAX_PITCH;
	}
}

void ThirdPersonCamera::ClampDistance()
{
	if (m_orbitDistance < MIN_DISTANCE) {
		m_orbitDistance = MIN_DISTANCE;
	}
	if (m_orbitDistance > MAX_DISTANCE) {
		m_orbitDistance = MAX_DISTANCE;
	}
}

void ThirdPersonCamera::HandleSDLEvent(SDL_Event* p_event)
{
	switch (p_event->type) {
	case SDL_EVENT_MOUSE_WHEEL:
		m_orbitDistance -= p_event->wheel.y * 0.5f;
		ClampDistance();
		break;

	case SDL_EVENT_MOUSE_MOTION:
		if (p_event->motion.state & SDL_BUTTON_RMASK) {
			m_orbitYaw += p_event->motion.xrel * 0.005f;
			m_orbitPitch += p_event->motion.yrel * 0.005f;
			ClampPitch();
		}
		break;

	case SDL_EVENT_FINGER_DOWN: {
		if (m_touch.count < 2) {
			int idx = m_touch.count;
			m_touch.id[idx] = p_event->tfinger.fingerID;
			m_touch.x[idx] = p_event->tfinger.x;
			m_touch.y[idx] = p_event->tfinger.y;
			m_touch.count++;

			if (m_touch.count == 2) {
				m_touchGestureActive = true;
				float dx = m_touch.x[1] - m_touch.x[0];
				float dy = m_touch.y[1] - m_touch.y[0];
				m_touch.initialPinchDist = SDL_sqrtf(dx * dx + dy * dy);
			}
		}
		break;
	}

	case SDL_EVENT_FINGER_MOTION: {
		if (m_touch.count == 2) {
			// Find which finger moved
			int idx = -1;
			for (int i = 0; i < 2; i++) {
				if (m_touch.id[i] == p_event->tfinger.fingerID) {
					idx = i;
					break;
				}
			}
			if (idx < 0) {
				break;
			}

			float oldX = m_touch.x[idx];
			float oldY = m_touch.y[idx];
			m_touch.x[idx] = p_event->tfinger.x;
			m_touch.y[idx] = p_event->tfinger.y;

			// Pinch zoom
			float dx = m_touch.x[1] - m_touch.x[0];
			float dy = m_touch.y[1] - m_touch.y[0];
			float newDist = SDL_sqrtf(dx * dx + dy * dy);

			if (m_touch.initialPinchDist > 0.001f) {
				float pinchDelta = m_touch.initialPinchDist - newDist;
				m_orbitDistance += pinchDelta * 15.0f;
				ClampDistance();
				m_touch.initialPinchDist = newDist;
			}

			// Two-finger drag for orbit
			float moveX = m_touch.x[idx] - oldX;
			float moveY = m_touch.y[idx] - oldY;
			m_orbitYaw -= moveX * 2.0f;
			m_orbitPitch += moveY * 2.0f;
			ClampPitch();
		}
		break;
	}

	case SDL_EVENT_FINGER_UP:
	case SDL_EVENT_FINGER_CANCELED: {
		for (int i = 0; i < m_touch.count; i++) {
			if (m_touch.id[i] == p_event->tfinger.fingerID) {
				// Shift remaining finger down
				if (i == 0 && m_touch.count == 2) {
					m_touch.id[0] = m_touch.id[1];
					m_touch.x[0] = m_touch.x[1];
					m_touch.y[0] = m_touch.y[1];
				}
				m_touch.count--;
				if (m_touch.count == 0) {
					m_touchGestureActive = false;
				}
				m_touch.initialPinchDist = 0.0f;
				break;
			}
		}
		break;
	}

	default:
		break;
	}
}

void ThirdPersonCamera::ReinitForCharacter()
{
	DestroyNameBubble();

	LegoPathActor* userActor = UserActor();
	if (!userActor) {
		m_active = false;
		return;
	}

	LegoROI* roi = userActor->GetROI();
	if (!roi) {
		m_active = false;
		return;
	}

	int8_t vehicleType = DetectVehicleType(userActor);

	// Large vehicles and helicopter: stay first-person
	if (vehicleType == VEHICLE_HELICOPTER || (vehicleType != VEHICLE_NONE && IsLargeVehicle(vehicleType))) {
		m_active = false;
		return;
	}

	m_animator.SetCurrentVehicleType(vehicleType);

	if (vehicleType != VEHICLE_NONE) {
		if (!EnsureDisplayROI()) {
			m_active = false;
			return;
		}

		if (!m_playerROI) {
			m_active = false;
			return;
		}

		// Undo TurnAround on the vehicle ROI so the backward-z convention
		// is restored.  This handles both entering from 1st-person (Enter's
		// TurnAround still in effect) and the Disable→Enable cycle (Disable
		// re-applied TurnAround).  In both cases ROI z currently matches
		// the visual forward and needs to be flipped back.
		{
			LegoROI* vehicleROI = userActor->GetROI();
			if (vehicleROI) {
				FlipROIDirection(vehicleROI);
			}
			m_roiUnflipped = false;
		}

		VideoManager()->Get3DManager()->Remove(*m_playerROI);
		VideoManager()->Get3DManager()->Add(*m_playerROI);
		m_active = true;
		SetupCamera(userActor);
		m_animator.BuildRideAnimation(vehicleType, m_playerROI, 0);
		CreateNameBubble();
		return;
	}

	// Reinitializing for walking character
	roi->SetVisibility(FALSE);
	if (!EnsureDisplayROI()) {
		m_active = false;
		return;
	}

	// Re-apply TurnAround if we undid it in Disable().
	// Only set the local matrix here; the subsequent Add() will propagate world data.
	// Flip the native ROI (not the display clone) since Tick() syncs the
	// clone's transform from it.
	if (m_roiUnflipped) {
		FlipROIDirection(roi);
		m_roiUnflipped = false;
	}

	m_playerROI->SetVisibility(TRUE);

	// Ensure the ROI is in the 3D manager.
	VideoManager()->Get3DManager()->Remove(*m_playerROI);
	VideoManager()->Get3DManager()->Add(*m_playerROI);

	m_animator.InitAnimCaches(m_playerROI);
	m_animator.ResetAnimState();
	m_active = true;

	m_animator.ApplyIdleFrame0(m_playerROI);
	SetupCamera(userActor);
	CreateNameBubble();
}
