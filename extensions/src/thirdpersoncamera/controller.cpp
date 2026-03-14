#include "extensions/thirdpersoncamera/controller.h"

#include "3dmanager/lego3dmanager.h"
#include "anim/legoanim.h"
#include "extensions/common/animutils.h"
#include "extensions/common/charactercustomizer.h"
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

Controller::Controller()
	: m_animator(CharacterAnimatorConfig{/*.saveEmoteTransform=*/true, /*.propSuffix=*/0}), m_enabled(false),
	  m_active(false), m_pendingWorldTransition(false), m_playerROI(nullptr)
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

	if (m_active && m_playerROI) {
		m_playerROI->SetVisibility(FALSE);
		VideoManager()->Get3DManager()->Remove(*m_playerROI);

		m_orbit.RestoreFirstPersonCamera();
	}

	m_active = false;
	m_pendingWorldTransition = false;
	m_animator.StopROISounds();
	m_animator.StopClickAnimation();
	m_display.DestroyDisplayClone();
	m_playerROI = nullptr;
	m_animator.ClearRideAnimation();
	m_animator.ClearAll();

	m_orbit.ResetOrbitState();
	if (!p_preserveTouch) {
		m_input.ResetTouchState();
	}
}

void Controller::OnActorEnter(IslePathActor* p_actor)
{
	LegoPathActor* userActor = UserActor();
	if (static_cast<LegoPathActor*>(p_actor) != userActor) {
		return;
	}

	m_animator.SetCurrentVehicleType(DetectVehicleType(userActor));

	if (!m_enabled) {
		return;
	}

	if (m_pendingWorldTransition && m_active) {
		return;
	}

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
		m_orbit.SetupCamera(userActor);
		m_animator.BuildRideAnimation(m_animator.GetCurrentVehicleType(), m_playerROI, 0);
		return;
	}

	newROI->SetVisibility(FALSE);
	if (!m_display.EnsureDisplayROI()) {
		return;
	}
	m_playerROI = m_display.GetDisplayROI();
	m_active = true;

	m_playerROI->SetVisibility(TRUE);

	VideoManager()->Get3DManager()->Remove(*m_playerROI);
	VideoManager()->Get3DManager()->Add(*m_playerROI);

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

	if (m_animator.GetCurrentVehicleType() != VEHICLE_NONE) {
		m_animator.ClearRideAnimation();
		m_animator.ClearAll();
		ReinitForCharacter();
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

	if (!UserActor() || UserActor()->GetActorState() != LegoPathActor::c_disabled) {
		m_orbit.ApplyOrbitCamera();
	}

	// Small vehicle with ride animation
	if (m_animator.GetCurrentVehicleType() != VEHICLE_NONE) {
		m_animator.StopClickAnimation();
		if (m_animator.GetRideAnim() && m_animator.GetRideRoiMap()) {
			LegoPathActor* actor = UserActor();
			if (!actor || !actor->GetROI()) {
				return;
			}

			AnimUtils::EnsureROIMapVisibility(m_animator.GetRideRoiMap(), m_animator.GetRideRoiMapSize());

			float speed = actor->GetWorldSpeed();
			if (SDL_fabsf(speed) > 0.01f) {
				m_animator.SetAnimTime(m_animator.GetAnimTime() + p_deltaTime * 2000.0f);
			}

			MxMatrix transform(actor->GetROI()->GetLocal2World());
			AnimUtils::FlipMatrixDirection(transform);

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
	if (m_display.GetDisplayROI() && m_display.GetDisplayROI() == m_playerROI) {
		m_display.SyncTransformFromNative(userActor->GetROI());
	}

	float speed = userActor->GetWorldSpeed();
	bool isMoving = SDL_fabsf(speed) > 0.01f;
	if (m_animator.IsInMultiPartEmote()) {
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

bool Controller::IsInMultiPartEmote() const
{
	return m_animator.IsInMultiPartEmote();
}

int8_t Controller::GetFrozenEmoteId() const
{
	return m_animator.GetFrozenEmoteId();
}

void Controller::TriggerEmote(uint8_t p_emoteId)
{
	if (!m_active) {
		return;
	}

	LegoPathActor* userActor = UserActor();
	if (!userActor) {
		return;
	}

	bool isMoving = SDL_fabsf(userActor->GetWorldSpeed()) > 0.01f;
	if (m_animator.IsInMultiPartEmote()) {
		isMoving = false;
	}
	m_animator.TriggerEmote(p_emoteId, m_playerROI, isMoving);
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
		m_animator.IsInMultiPartEmote()
	);
}

void Controller::HandleSDLEventImpl(SDL_Event* p_event)
{
	m_input.HandleSDLEvent(p_event, m_orbit, m_active);
}

void Controller::ReinitForCharacter()
{
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

		VideoManager()->Get3DManager()->Remove(*m_playerROI);
		VideoManager()->Get3DManager()->Add(*m_playerROI);
		m_active = true;
		m_orbit.SetupCamera(userActor);
		m_animator.BuildRideAnimation(vehicleType, m_playerROI, 0);
		return;
	}

	roi->SetVisibility(FALSE);
	if (!m_display.EnsureDisplayROI()) {
		m_active = false;
		return;
	}
	m_playerROI = m_display.GetDisplayROI();

	m_playerROI->SetVisibility(TRUE);

	VideoManager()->Get3DManager()->Remove(*m_playerROI);
	VideoManager()->Get3DManager()->Add(*m_playerROI);

	m_animator.InitAnimCaches(m_playerROI);
	m_animator.ResetAnimState();
	m_active = true;

	m_animator.ApplyIdleFrame0(m_playerROI);

	if (!m_pendingWorldTransition) {
		m_orbit.SetupCamera(userActor);
	}
}
