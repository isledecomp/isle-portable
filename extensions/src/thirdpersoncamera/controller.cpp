#include "extensions/thirdpersoncamera/controller.h"

#include "3dmanager/lego3dmanager.h"
#include "anim/legoanim.h"
#include "extensions/common/animutils.h"
#include "extensions/common/arearestriction.h"
#include "extensions/common/charactercustomizer.h"
#include "extensions/common/charactertables.h"
#include "extensions/common/constants.h"
#include "islepathactor.h"
#include "legoactor.h"
#include "legoactors.h"
#include "legocameracontroller.h"
#include "legonavcontroller.h"
#include "legopathactor.h"
#include "legovideomanager.h"
#include "legoworld.h"
#include "misc.h"
#include "misc/legotree.h"
#include "mxgeometry/mxgeometry3d.h"
#include "mxgeometry/mxmatrix.h"
#include "mxmisc.h"
#include "realtime/vector.h"
#include "roi/legoroi.h"

#include <SDL3/SDL_stdinc.h>

using namespace Extensions;
using namespace Extensions::Common;
using namespace Extensions::ThirdPersonCamera;

static constexpr float SPEED_EPSILON = 0.01f;

static void ReaddROI(LegoROI& p_roi)
{
	VideoManager()->Get3DManager()->Remove(p_roi);
	VideoManager()->Get3DManager()->Add(p_roi);
}

Controller::Controller()
	: m_animator(CharacterAnimatorConfig{/*.saveExtraAnimTransform=*/true, /*.propSuffix=*/0}), m_enabled(false),
	  m_active(false), m_pendingWorldTransition(false), m_animPlaying(false), m_animLockDisplay(false),
	  m_lmbForwardEngaged(false), m_playerROI(nullptr)
{
}

void Controller::Enable()
{
	m_enabled = true;
	ReinitForCharacter();
}

void Controller::Disable(bool p_preserveTouch)
{
	m_enabled = false;
	Deactivate();
	if (!p_preserveTouch) {
		m_input.ResetTouchState();
	}
}

void Controller::CancelExternalAnim()
{
	if (m_animPlaying) {
		if (m_animStopCallback) {
			m_animStopCallback();
		}
		m_animPlaying = false;
		m_animStopCallback = nullptr;
	}
}

void Controller::Deactivate()
{
	// Stop external animation before destroying the display ROI
	CancelExternalAnim();

	if (m_active && m_playerROI) {
		m_playerROI->SetVisibility(FALSE);
		VideoManager()->Get3DManager()->Remove(*m_playerROI);
		m_orbit.RestoreFirstPersonCamera();
	}

	m_active = false;
	m_pendingWorldTransition = false;
	m_lmbForwardEngaged = false;
	m_animator.StopROISounds();
	m_animator.StopClickAnimation();
	m_display.DestroyDisplayClone();
	m_playerROI = nullptr;
	m_animator.ClearRideAnimation();
	m_animator.ClearAll();
	m_orbit.ResetOrbitState();
}

void Controller::OnActorEnter(IslePathActor* p_actor)
{
	LegoPathActor* userActor = UserActor();
	if (static_cast<LegoPathActor*>(p_actor) != userActor) {
		return;
	}

	// Prevent the previous actor from wandering on the path system with stale
	// spline state while the player is in a vehicle. Exit() will later call
	// SetBoundary() without updating m_destEdge, so any non-user-nav animation
	// with the old spline would use a mismatched boundary/edge pair.
	if (p_actor->m_previousActor) {
		p_actor->m_previousActor->SetWorldSpeed(0);
	}

	m_animator.SetCurrentVehicleType(DetectVehicleType(userActor));

	if (!m_enabled || IsRestrictedArea(GameState()->m_currentArea)) {
		return;
	}

	if (m_pendingWorldTransition && m_active) {
		return;
	}

	// Stop external animation before modifying ride/display state —
	// the animation caller may hold a reference to the ride vehicle ROI.
	CancelExternalAnim();

	LegoROI* newROI = userActor->GetROI();
	if (!newROI) {
		return;
	}

	if (m_animator.GetCurrentVehicleType() != VEHICLE_NONE) {
		if (IsLargeVehicle(m_animator.GetCurrentVehicleType()) ||
			m_animator.GetCurrentVehicleType() == VEHICLE_HELICOPTER) {
			if (m_playerROI) {
				m_playerROI->SetVisibility(FALSE);
				VideoManager()->Get3DManager()->Remove(*m_playerROI);
			}
			m_active = false;
			return;
		}

		if (!m_playerROI) {
			return;
		}

		m_active = true;
		m_orbit.ResetSmoothedSpeed();
		m_orbit.ApplyOrbitCamera();
		m_animator.BuildRideAnimation(m_animator.GetCurrentVehicleType(), m_playerROI);
		return;
	}

	newROI->SetVisibility(FALSE);
	if (!m_display.EnsureDisplayROI()) {
		return;
	}
	m_playerROI = m_display.GetDisplayROI();
	m_active = true;

	m_playerROI->SetVisibility(TRUE);

	ReaddROI(*m_playerROI);

	m_animator.InitAnimCaches(m_playerROI);
	m_animator.ResetAnimState();

	m_animator.ApplyIdleFrame0(m_playerROI);

	m_orbit.SetupCamera(userActor);
}

void Controller::OnActorExit(IslePathActor* p_actor)
{
	if (!m_enabled) {
		return;
	}

	// Stop external animation before clearing ride animation state —
	// the animation caller may hold a reference to the ride vehicle ROI.
	CancelExternalAnim();

	if (m_animator.GetCurrentVehicleType() != VEHICLE_NONE) {
		m_animator.ClearRideAnimation();
		m_animator.ClearAll();
		ReinitForCharacter(/*p_preserveCamera=*/true);
	}
	else if (m_active && static_cast<LegoPathActor*>(p_actor) == UserActor()) {
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

void Controller::OnCamAnimEnd(LegoPathActor* p_actor)
{
	m_pendingWorldTransition = false;

	if (!m_active) {
		return;
	}

	m_orbit.SetupCamera(p_actor);
}

void Controller::Tick(float p_deltaTime)
{
	if (IsRestrictedArea(GameState()->m_currentArea)) {
		return;
	}

	if (!m_display.IsDisplayActorFrozen()) {
		LegoPathActor* userActor = UserActor();
		if (userActor) {
			uint8_t actorId = static_cast<LegoActor*>(userActor)->GetActorId();
			if (IsValidActorId(actorId)) {
				uint8_t derived = actorId - 1;
				if (derived != m_display.GetDisplayActorIndex()) {
					m_display.SetDisplayActorIndex(derived);
				}
			}
		}
	}

	if (!m_active) {
		return;
	}

	if (!m_playerROI) {
		return;
	}

	if (m_pendingWorldTransition) {
		m_pendingWorldTransition = false;
		LegoPathActor* actor = UserActor();
		if (actor && actor->GetROI()) {
			m_orbit.InitAbsoluteYaw(actor->GetROI());
		}
	}

	if ((!m_animPlaying || !m_animLockDisplay) &&
		(!UserActor() || UserActor()->GetActorState() != LegoPathActor::c_disabled)) {
		m_orbit.ApplyOrbitCamera();
	}

	// Small vehicle with ride animation (skip when external animation is active —
	// the animation controller handles positioning the player and vehicle ROI)
	if (m_animator.GetCurrentVehicleType() != VEHICLE_NONE && !m_animPlaying) {
		m_animator.StopClickAnimation();
		if (m_animator.GetRideAnim() && m_animator.GetRideRoiMap()) {
			LegoPathActor* actor = UserActor();
			if (!actor || !actor->GetROI()) {
				return;
			}

			AnimUtils::EnsureROIMapVisibility(m_animator.GetRideRoiMap(), m_animator.GetRideRoiMapSize());

			float speed = actor->GetWorldSpeed();
			if (SDL_fabsf(speed) > SPEED_EPSILON) {
				m_animator.SetAnimTime(m_animator.GetAnimTime() + p_deltaTime * CharacterAnimator::ANIM_TIME_SCALE);
			}

			MxMatrix transform(actor->GetROI()->GetLocal2World());
			AnimUtils::FlipMatrixDirection(transform);

			m_playerROI->WrappedSetLocal2WorldWithWorldDataUpdate(transform);
			m_playerROI->SetVisibility(TRUE);

			float duration = (float) m_animator.GetRideAnim()->GetDuration();
			if (duration > 0.0f) {
				float timeInCycle =
					m_animator.GetAnimTime() - duration * SDL_floorf(m_animator.GetAnimTime() / duration);

				AnimUtils::ApplyTree(
					m_animator.GetRideAnim(),
					transform,
					(LegoTime) timeInCycle,
					m_animator.GetRideRoiMap()
				);
			}
		}
		return;
	}

	LegoPathActor* userActor = UserActor();
	if (!userActor) {
		return;
	}

	// When performing in an external animation, prevent movement and skip display updates
	// (the animation drives positioning). Spectators are free to move.
	if (m_animPlaying && m_animLockDisplay) {
		userActor->SetWorldSpeed(0.0f);
		NavController()->SetLinearVel(0.0f);
		return;
	}

	// Sync display clone position from native ROI
	if (m_display.GetDisplayROI() && m_display.GetDisplayROI() == m_playerROI) {
		m_display.SyncTransformFromNative(userActor->GetROI());
	}

	float speed = userActor->GetWorldSpeed();
	bool isMoving = SDL_fabsf(speed) > SPEED_EPSILON;
	if (m_animator.IsExtraAnimBlocking()) {
		isMoving = false;
		userActor->SetWorldSpeed(0.0f);
		NavController()->SetLinearVel(0.0f);
	}

	m_animator.Tick(p_deltaTime, m_playerROI, isMoving);
}

void Controller::SetWalkAnimId(uint8_t p_walkAnimId)
{
	m_animator.SetWalkAnimId(p_walkAnimId, m_active ? m_playerROI : nullptr);
}

void Controller::SetIdleAnimId(uint8_t p_idleAnimId)
{
	m_animator.SetIdleAnimId(p_idleAnimId, m_active ? m_playerROI : nullptr);
}

bool Controller::IsExtraAnimBlocking() const
{
	return m_animator.IsExtraAnimBlocking();
}

int8_t Controller::GetFrozenExtraAnimId() const
{
	return m_animator.GetFrozenExtraAnimId();
}

void Controller::TriggerExtraAnim(uint8_t p_id)
{
	if (!m_active) {
		return;
	}

	LegoPathActor* userActor = UserActor();
	if (!userActor) {
		return;
	}

	bool isMoving = SDL_fabsf(userActor->GetWorldSpeed()) > SPEED_EPSILON;
	if (m_animator.IsExtraAnimBlocking()) {
		isMoving = false;
	}
	m_animator.TriggerExtraAnim(p_id, m_playerROI, isMoving);
}

void Controller::StopClickAnimation()
{
	m_animator.StopClickAnimation();
}

void Controller::OnWorldEnabled(LegoWorld* p_world)
{
	if (!p_world) {
		return;
	}

	if (!m_enabled) {
		return;
	}

	if (IsRestrictedArea(GameState()->m_currentArea)) {
		Deactivate();
		m_input.ResetTouchState();
		return;
	}

	m_animator.ClearAll();

	m_orbit.ResetOrbitState();

	m_pendingWorldTransition = true;

	ReinitForCharacter();
}

void Controller::OnWorldDisabled(LegoWorld* p_world)
{
	if (!p_world) {
		return;
	}

	// Stop external animation before destroying the display ROI
	CancelExternalAnim();

	m_active = false;
	m_pendingWorldTransition = false;
	m_playerROI = nullptr;
	m_animator.StopROISounds();
	m_animator.StopClickAnimation();
	m_display.DestroyDisplayClone();
	m_animator.ClearRideAnimation();
	m_animator.ClearAll();
}

MxBool Controller::HandleCameraRelativeMovement(
	LegoNavController* p_nav,
	const Vector3& p_curPos,
	const Vector3& p_curDir,
	Vector3& p_newPos,
	Vector3& p_newDir,
	float p_deltaTime
)
{
	return m_orbit.HandleCameraRelativeMovement(
		p_nav,
		p_curPos,
		p_curDir,
		p_newPos,
		p_newDir,
		p_deltaTime,
		m_animator.IsExtraAnimBlocking() || (m_animPlaying && m_animLockDisplay),
		m_input.IsLmbHeldForMovement()
	);
}

void Controller::HandleSDLEventImpl(SDL_Event* p_event)
{
	m_input.HandleSDLEvent(p_event, m_orbit, m_active);
}

MxBool Controller::HandleFirstPersonForward(
	LegoNavController* p_nav,
	const Vector3& p_curPos,
	const Vector3& p_curDir,
	Vector3& p_newPos,
	Vector3& p_newDir,
	float p_deltaTime
)
{
	float accel = p_nav->m_maxLinearAccel;
	p_nav->m_linearVel += accel * p_deltaTime;
	if (p_nav->m_linearVel > p_nav->m_maxLinearVel) {
		p_nav->m_linearVel = p_nav->m_maxLinearVel;
	}

	float speed = p_nav->m_linearVel * p_deltaTime;
	p_newPos[0] = p_curPos[0] + p_curDir[0] * speed;
	p_newPos[1] = p_curPos[1] + p_curDir[1] * speed;
	p_newPos[2] = p_curPos[2] + p_curDir[2] * speed;
	p_newDir = p_curDir;

	p_nav->m_rotationalVel = 0.0f;
	return TRUE;
}

void Controller::ReinitForCharacter(bool p_preserveCamera)
{
	if (!GameState() || IsRestrictedArea(GameState()->m_currentArea)) {
		m_active = false;
		return;
	}

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

	if (vehicleType == VEHICLE_HELICOPTER || (vehicleType != VEHICLE_NONE && IsLargeVehicle(vehicleType))) {
		m_active = false;
		m_pendingWorldTransition = false;
		return;
	}

	m_animator.SetCurrentVehicleType(vehicleType);

	if (vehicleType != VEHICLE_NONE) {
		if (!m_display.EnsureDisplayROI()) {
			m_active = false;
			return;
		}
		m_playerROI = m_display.GetDisplayROI();

		if (!m_playerROI) {
			m_active = false;
			return;
		}

		m_pendingWorldTransition = false;

		ReaddROI(*m_playerROI);
		m_active = true;
		m_orbit.SetupCamera(userActor);
		m_animator.BuildRideAnimation(vehicleType, m_playerROI);
		return;
	}

	roi->SetVisibility(FALSE);
	if (!m_display.EnsureDisplayROI()) {
		m_active = false;
		return;
	}
	m_playerROI = m_display.GetDisplayROI();

	m_playerROI->SetVisibility(TRUE);

	ReaddROI(*m_playerROI);

	m_animator.InitAnimCaches(m_playerROI);
	m_animator.ResetAnimState();
	m_active = true;

	m_animator.ApplyIdleFrame0(m_playerROI);

	if (!m_pendingWorldTransition) {
		if (p_preserveCamera) {
			m_orbit.ResetSmoothedSpeed();
			m_orbit.ApplyOrbitCamera();
		}
		else {
			m_orbit.SetupCamera(userActor);
		}
	}
}
