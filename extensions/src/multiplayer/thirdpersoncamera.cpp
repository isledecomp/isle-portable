#include "extensions/multiplayer/thirdpersoncamera.h"

#include "3dmanager/lego3dmanager.h"
#include "anim/legoanim.h"
#include "extensions/multiplayer/charactercloner.h"
#include "extensions/multiplayer/charactercustomizer.h"
#include "islepathactor.h"
#include "legocameracontroller.h"
#include "legocharactermanager.h"
#include "legoinputmanager.h"
#include "legonavcontroller.h"
#include "legopathactor.h"
#include "legovideomanager.h"
#include "legoworld.h"
#include "misc.h"
#include "misc/legotree.h"
#include "mxgeometry/mxgeometry3d.h"
#include "mxgeometry/mxmatrix.h"
#include "realtime/realtime.h"
#include "realtime/vector.h"
#include "roi/legoroi.h"

#include <SDL3/SDL_stdinc.h>

using namespace Multiplayer;

static constexpr float TURN_RATE = 10.0f;

// Flip a matrix from forward-z to backward-z (or vice versa) in place.
// Same operation as IslePathActor::TurnAround: negate z, recompute right.
static void FlipMatrixDirection(MxMatrix& p_mat)
{
	Vector3 right(p_mat[0]);
	Vector3 up(p_mat[1]);
	Vector3 direction(p_mat[2]);
	direction *= -1.0f;
	right.EqualsCross(up, direction);
}

ThirdPersonCamera::ThirdPersonCamera()
	: m_enabled(false), m_active(false), m_pendingWorldTransition(false), m_playerROI(nullptr),
	  m_displayActorIndex(DISPLAY_ACTOR_NONE), m_displayROI(nullptr),
	  m_animator(CharacterAnimatorConfig{/*.saveEmoteTransform=*/true}), m_showNameBubble(true),
	  m_orbitPitch(DEFAULT_ORBIT_PITCH), m_orbitDistance(DEFAULT_ORBIT_DISTANCE),
	  m_absoluteYaw(DEFAULT_ORBIT_YAW), m_smoothedSpeed(0.0f), m_touch{}
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
	m_pendingWorldTransition = false;
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

	if (!m_enabled) {
		return;
	}

	// During a world transition, the ROI position is stale (PlaceActor hasn't
	// run yet).  Skip camera setup — the stale orbit view would freeze on
	// screen during the ~500ms world load.  ApplyOrbitCamera in the first
	// Tick after PlaceActor handles camera setup naturally.
	if (m_pendingWorldTransition && m_active) {
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

		m_active = true;
		SetupCamera(userActor);
		m_animator.BuildRideAnimation(m_animator.GetCurrentVehicleType(), m_playerROI, 0);
		CreateNameBubble();
		return;
	}

	// Walking character entry.
	newROI->SetVisibility(FALSE);
	if (!EnsureDisplayROI()) {
		return;
	}
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
	m_pendingWorldTransition = false;

	if (!m_active) {
		return;
	}

	// Cam anim end placed the actor via PlaceActor (forward-z).
	// Restore the orbit camera.
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

	// After a world transition, PlaceActor has now run and set the ROI to
	// the correct position.  Clear the flag so subsequent OnActorEnter calls
	// work normally.  Initialize absolute yaw from the player's actual
	// direction so the camera starts behind the character.
	if (m_pendingWorldTransition) {
		m_pendingWorldTransition = false;
		LegoPathActor* actor = UserActor();
		if (actor && actor->GetROI()) {
			InitAbsoluteYaw(actor->GetROI());
		}
	}

	// While a cam anim locks the player (actor state c_disabled), calling
	// ApplyOrbitCamera would fight the cam anim each frame and, critically,
	// if the cam anim is interrupted (space bar), its end handler reads
	// ViewROI position to place the actor.  Our orbit camera position
	// (elevated, behind player) would cause the actor to be placed in the
	// air.  Once the player is released (first interruption resets actor
	// state to c_initial), the orbit camera resumes immediately.
	if (!UserActor() || UserActor()->GetActorState() != LegoPathActor::c_disabled) {
		ApplyOrbitCamera();
	}

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

			// Use vehicle actor's transform as base, flipped to backward-z
			// so the character mesh (which faces -z) renders correctly.
			MxMatrix transform(actor->GetROI()->GetLocal2World());
			FlipMatrixDirection(transform);

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
			// Native ROI uses forward-z; flip to backward-z for the mesh.
			FlipMatrixDirection(mat);
			m_displayROI->WrappedSetLocal2WorldWithWorldDataUpdate(mat);
			VideoManager()->Get3DManager()->Moved(*m_displayROI);
		}
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

void ThirdPersonCamera::SetWalkAnimId(uint8_t p_walkAnimId)
{
	m_animator.SetWalkAnimId(p_walkAnimId, m_active ? m_playerROI : nullptr);
}

void ThirdPersonCamera::SetIdleAnimId(uint8_t p_idleAnimId)
{
	m_animator.SetIdleAnimId(p_idleAnimId, m_active ? m_playerROI : nullptr);
}

bool ThirdPersonCamera::IsInMultiPartEmote() const
{
	return m_animator.IsInMultiPartEmote();
}

int8_t ThirdPersonCamera::GetFrozenEmoteId() const
{
	return m_animator.GetFrozenEmoteId();
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

	bool isMoving = SDL_fabsf(userActor->GetWorldSpeed()) > 0.01f;
	if (m_animator.IsInMultiPartEmote()) {
		isMoving = false;
	}
	m_animator.TriggerEmote(p_emoteId, m_playerROI, isMoving);
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
	if (!p_world) {
		return;
	}

	if (!m_enabled) {
		return;
	}

	// Animation presenters may have been recreated.
	m_animator.ClearAll();

	// Reset orbit to default position behind the character.
	ResetOrbitState();

	// ReinitForCharacter runs BEFORE SpawnPlayer/PlaceActor, so the ROI
	// position is stale.  Set the flag so OnActorEnter and ReinitForCharacter
	// defer camera setup.  The first Tick after PlaceActor clears it, and
	// ApplyOrbitCamera handles the camera naturally.
	m_pendingWorldTransition = true;

	ReinitForCharacter();
}

void ThirdPersonCamera::OnWorldDisabled(LegoWorld* p_world)
{
	if (!p_world) {
		return;
	}
	m_active = false;
	m_pendingWorldTransition = false;
	m_playerROI = nullptr;
	DestroyNameBubble();
	DestroyDisplayClone();
	m_animator.ClearRideAnimation();
	m_animator.ClearAll();
}

float ThirdPersonCamera::GetLocalYaw(LegoROI* p_roi) const
{
	if (p_roi) {
		const float* dir = p_roi->GetWorldDirection();
		float playerWorldYaw = SDL_atan2f(-dir[0], dir[2]);
		return m_absoluteYaw - playerWorldYaw;
	}
	return m_absoluteYaw;
}

void ThirdPersonCamera::InitAbsoluteYaw(LegoROI* p_roi)
{
	const float* dir = p_roi->GetWorldDirection();
	m_absoluteYaw = SDL_atan2f(-dir[0], dir[2]) + DEFAULT_ORBIT_YAW;
}

void ThirdPersonCamera::SetupCamera(LegoPathActor* p_actor)
{
	LegoWorld* world = CurrentWorld();
	if (!world || !world->GetCameraController()) {
		return;
	}

	LegoROI* roi = p_actor->GetROI();
	if (roi) {
		InitAbsoluteYaw(roi);
	}
	m_smoothedSpeed = 0.0f;

	// InitAbsoluteYaw sets m_absoluteYaw = playerYaw + DEFAULT_ORBIT_YAW,
	// so localYaw = m_absoluteYaw - playerYaw = DEFAULT_ORBIT_YAW.
	Mx3DPointFloat at, camDir, up;
	ComputeOrbitVectors(DEFAULT_ORBIT_YAW, at, camDir, up);

	world->GetCameraController()->SetWorldTransform(at, camDir, up);
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

void ThirdPersonCamera::ComputeOrbitVectors(
	float p_yaw,
	Mx3DPointFloat& p_at,
	Mx3DPointFloat& p_dir,
	Mx3DPointFloat& p_up
) const
{
	// Convert spherical coordinates to camera offset in entity-local space.
	// The ROI uses forward-z (Z+ = visual forward).  The camera orbits
	// behind the character, so at yaw=0 it sits at local -Z.
	float cosP = SDL_cosf(m_orbitPitch);
	float sinP = SDL_sinf(m_orbitPitch);
	float sinY = SDL_sinf(p_yaw);
	float cosY = SDL_cosf(p_yaw);

	p_at = Mx3DPointFloat(
		m_orbitDistance * sinY * cosP,
		ORBIT_TARGET_HEIGHT + m_orbitDistance * sinP,
		-m_orbitDistance * cosY * cosP
	);

	// Direction points from camera toward the pivot (the character).
	p_dir = Mx3DPointFloat(-sinY * cosP, -sinP, cosY * cosP);

	p_up = Mx3DPointFloat(0.0f, 1.0f, 0.0f);
}

void ThirdPersonCamera::ApplyOrbitCamera()
{
	LegoPathActor* actor = UserActor();
	LegoWorld* world = CurrentWorld();
	if (!actor || !world || !world->GetCameraController()) {
		return;
	}

	// Derive entity-local yaw from absolute yaw and player's world facing.
	// This prevents the camera from rotating when the player turns.
	float localYaw = GetLocalYaw(actor->GetROI());

	Mx3DPointFloat at, camDir, up;
	ComputeOrbitVectors(localYaw, at, camDir, up);

	world->GetCameraController()->SetWorldTransform(at, camDir, up);
	actor->TransformPointOfView();
}

void ThirdPersonCamera::ResetOrbitState()
{
	m_orbitPitch = DEFAULT_ORBIT_PITCH;
	m_orbitDistance = DEFAULT_ORBIT_DISTANCE;
	m_absoluteYaw = DEFAULT_ORBIT_YAW;
	m_smoothedSpeed = 0.0f;
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

MxBool ThirdPersonCamera::HandleCameraRelativeMovement(
	LegoNavController* p_nav,
	const Vector3& p_curPos,
	const Vector3& p_curDir,
	Vector3& p_newPos,
	Vector3& p_newDir,
	float p_deltaTime
)
{
	// Read keyboard state
	LegoInputManager* inputManager = InputManager();
	MxU32 keyFlags = 0;
	if (!inputManager || inputManager->GetNavigationKeyStates(keyFlags) == FAILURE) {
		keyFlags = 0;
	}

	// Compute camera world-forward and right from absolute yaw
	float camForwardX = -SDL_sinf(m_absoluteYaw);
	float camForwardZ = SDL_cosf(m_absoluteYaw);
	float camRightX = SDL_cosf(m_absoluteYaw);
	float camRightZ = SDL_sinf(m_absoluteYaw);

	// Map key flags to combined movement direction
	float moveDirX = 0.0f;
	float moveDirZ = 0.0f;

	if (keyFlags & LegoInputManager::c_up) {
		moveDirX += camForwardX;
		moveDirZ += camForwardZ;
	}
	if (keyFlags & LegoInputManager::c_down) {
		moveDirX -= camForwardX;
		moveDirZ -= camForwardZ;
	}
	if (keyFlags & LegoInputManager::c_left) {
		moveDirX -= camRightX;
		moveDirZ -= camRightZ;
	}
	if (keyFlags & LegoInputManager::c_right) {
		moveDirX += camRightX;
		moveDirZ += camRightZ;
	}

	// Normalize movement direction
	float moveDirLen = SDL_sqrtf(moveDirX * moveDirX + moveDirZ * moveDirZ);
	bool hasInput = moveDirLen > 0.001f;
	if (hasInput) {
		moveDirX /= moveDirLen;
		moveDirZ /= moveDirLen;
	}

	// Smooth speed using acceleration/deceleration (mirroring nav controller's model)
	float maxSpeed = p_nav->m_maxLinearVel;
	if (hasInput) {
		float accel = p_nav->m_maxLinearAccel;
		m_smoothedSpeed += accel * p_deltaTime;
		if (m_smoothedSpeed > maxSpeed) {
			m_smoothedSpeed = maxSpeed;
		}
	}
	else {
		float decel = p_nav->m_maxLinearDeccel;
		m_smoothedSpeed -= decel * p_deltaTime;
		if (m_smoothedSpeed < 0.0f) {
			m_smoothedSpeed = 0.0f;
		}
	}

	if (m_smoothedSpeed < p_nav->m_zeroThreshold && !hasInput) {
		m_smoothedSpeed = 0.0f;
		// No movement, keep current position and direction
		p_newPos = p_curPos;
		p_newDir = p_curDir;
	}
	else {
		// Compute new position.  Include p_curDir[1] (slope from boundary
		// orientation) so the actor follows terrain height changes.
		float speed = m_smoothedSpeed * p_deltaTime;
		if (hasInput) {
			p_newPos[0] = p_curPos[0] + moveDirX * speed;
			p_newPos[1] = p_curPos[1] + p_curDir[1] * speed;
			p_newPos[2] = p_curPos[2] + moveDirZ * speed;

			// Smooth turn: interpolate facing toward movement direction
			float targetYaw = SDL_atan2f(-moveDirX, moveDirZ);
			float currentYaw = SDL_atan2f(-p_curDir[0], p_curDir[2]);
			float angleDiff = targetYaw - currentYaw;

			// Wrap to [-PI, PI]
			while (angleDiff > SDL_PI_F) {
				angleDiff -= 2.0f * SDL_PI_F;
			}
			while (angleDiff < -SDL_PI_F) {
				angleDiff += 2.0f * SDL_PI_F;
			}

			float maxTurn = TURN_RATE * p_deltaTime;
			if (SDL_fabsf(angleDiff) > maxTurn) {
				angleDiff = angleDiff > 0 ? maxTurn : -maxTurn;
			}

			float newYaw = currentYaw + angleDiff;
			p_newDir[0] = -SDL_sinf(newYaw);
			p_newDir[1] = p_curDir[1];
			p_newDir[2] = SDL_cosf(newYaw);
		}
		else {
			// Decelerating: continue in current direction
			p_newPos[0] = p_curPos[0] + p_curDir[0] * speed;
			p_newPos[1] = p_curPos[1] + p_curDir[1] * speed;
			p_newPos[2] = p_curPos[2] + p_curDir[2] * speed;
			p_newDir = p_curDir;
		}
	}

	// Set nav controller velocities via friend access so GetWorldSpeed()
	// reports correctly for animations/network
	p_nav->m_linearVel = m_smoothedSpeed;
	// Suppress camera roll in Animate()
	p_nav->m_rotationalVel = 0.0f;

	// Pre-set camera controller's local transform for the NEW player direction.
	// TransformPointOfView() runs after this hook returns but before Tick()'s
	// ApplyOrbitCamera().  Without this, the stale local transform (computed for
	// the old facing) composes with the new actor transform, causing a one-frame
	// camera flash in the wrong direction.
	LegoWorld* world = CurrentWorld();
	if (world && world->GetCameraController()) {
		float newPlayerYaw = SDL_atan2f(-p_newDir[0], p_newDir[2]);
		float localYaw = m_absoluteYaw - newPlayerYaw;

		Mx3DPointFloat at, camDir, camUp;
		ComputeOrbitVectors(localYaw, at, camDir, camUp);

		world->GetCameraController()->SetWorldTransform(at, camDir, camUp);
	}

	return TRUE;
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
			m_absoluteYaw -= p_event->motion.xrel * 0.005f;
			m_orbitPitch += p_event->motion.yrel * 0.005f;
			ClampPitch();
		}
		break;

	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP: {
		SDL_Window* window = SDL_GetWindowFromID(p_event->button.windowID);
		if (window) {
			SDL_SetWindowRelativeMouseMode(window, SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_RMASK);
		}
		break;
	}

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
			m_absoluteYaw += moveX * 2.0f;
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
		m_pendingWorldTransition = false;
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

		m_pendingWorldTransition = false;

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

	m_playerROI->SetVisibility(TRUE);

	// Ensure the ROI is in the 3D manager.
	VideoManager()->Get3DManager()->Remove(*m_playerROI);
	VideoManager()->Get3DManager()->Add(*m_playerROI);

	m_animator.InitAnimCaches(m_playerROI);
	m_animator.ResetAnimState();
	m_active = true;

	m_animator.ApplyIdleFrame0(m_playerROI);

	// During a world transition, PlaceActor hasn't run yet — the ROI is at
	// a stale position.  Defer camera setup; ApplyOrbitCamera in the first
	// Tick after PlaceActor handles it.
	if (!m_pendingWorldTransition) {
		SetupCamera(userActor);
	}
	CreateNameBubble();
}
