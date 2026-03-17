#include "extensions/thirdpersoncamera/orbitcamera.h"

#include "extensions/common/characteranimator.h"
#include "legocameracontroller.h"
#include "legoinputmanager.h"
#include "legonavcontroller.h"
#include "legopathactor.h"
#include "legoworld.h"
#include "misc.h"
#include "mxgeometry/mxmatrix.h"
#include "realtime/vector.h"
#include "roi/legoroi.h"

using namespace Extensions::ThirdPersonCamera;

static constexpr float TURN_RATE = 10.0f;

OrbitCamera::OrbitCamera()
	: m_orbitPitch(DEFAULT_ORBIT_PITCH), m_orbitDistance(DEFAULT_ORBIT_DISTANCE),
	  m_absoluteYaw(DEFAULT_ORBIT_YAW), m_smoothedSpeed(0.0f)
{
}

void OrbitCamera::ComputeOrbitVectors(
	float p_yaw,
	Mx3DPointFloat& p_at,
	Mx3DPointFloat& p_dir,
	Mx3DPointFloat& p_up
) const
{
	float cosP = SDL_cosf(m_orbitPitch);
	float sinP = SDL_sinf(m_orbitPitch);
	float sinY = SDL_sinf(p_yaw);
	float cosY = SDL_cosf(p_yaw);

	p_at = Mx3DPointFloat(
		m_orbitDistance * sinY * cosP,
		ORBIT_TARGET_HEIGHT + m_orbitDistance * sinP,
		-m_orbitDistance * cosY * cosP
	);

	p_dir = Mx3DPointFloat(-sinY * cosP, -sinP, cosY * cosP);

	p_up = Mx3DPointFloat(0.0f, 1.0f, 0.0f);
}

float OrbitCamera::GetLocalYaw(LegoROI* p_roi) const
{
	if (p_roi) {
		const float* dir = p_roi->GetWorldDirection();
		float playerWorldYaw = SDL_atan2f(-dir[0], dir[2]);
		return m_absoluteYaw - playerWorldYaw;
	}
	return m_absoluteYaw;
}

void OrbitCamera::InitAbsoluteYaw(LegoROI* p_roi)
{
	const float* dir = p_roi->GetWorldDirection();
	m_absoluteYaw = SDL_atan2f(-dir[0], dir[2]) + DEFAULT_ORBIT_YAW;
}

void OrbitCamera::SetupCamera(LegoPathActor* p_actor)
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

	Mx3DPointFloat at, camDir, up;
	ComputeOrbitVectors(DEFAULT_ORBIT_YAW, at, camDir, up);

	world->GetCameraController()->SetWorldTransform(at, camDir, up);
	p_actor->TransformPointOfView();
}

void OrbitCamera::ApplyOrbitCamera()
{
	LegoPathActor* actor = UserActor();
	LegoWorld* world = CurrentWorld();
	if (!actor || !world || !world->GetCameraController()) {
		return;
	}

	float localYaw = GetLocalYaw(actor->GetROI());

	Mx3DPointFloat at, camDir, up;
	ComputeOrbitVectors(localYaw, at, camDir, up);

	world->GetCameraController()->SetWorldTransform(at, camDir, up);
	actor->TransformPointOfView();
}

void OrbitCamera::ResetOrbitState()
{
	m_orbitPitch = DEFAULT_ORBIT_PITCH;
	m_orbitDistance = DEFAULT_ORBIT_DISTANCE;
	m_absoluteYaw = DEFAULT_ORBIT_YAW;
	m_smoothedSpeed = 0.0f;
}

void OrbitCamera::ClampPitch()
{
	if (m_orbitPitch < MIN_PITCH) {
		m_orbitPitch = MIN_PITCH;
	}
	if (m_orbitPitch > MAX_PITCH) {
		m_orbitPitch = MAX_PITCH;
	}
}

void OrbitCamera::ClampDistance()
{
	if (m_orbitDistance < SWITCH_TO_FIRST_PERSON_DISTANCE) {
		m_orbitDistance = SWITCH_TO_FIRST_PERSON_DISTANCE;
	}
	if (m_orbitDistance > MAX_DISTANCE) {
		m_orbitDistance = MAX_DISTANCE;
	}
}

void OrbitCamera::RestoreFirstPersonCamera()
{
	LegoPathActor* userActor = UserActor();
	LegoWorld* world = CurrentWorld();

	if (userActor && world && world->GetCameraController()) {
		world->GetCameraController()->SetWorldTransform(
			Mx3DPointFloat(0.0F, 1.25F, 0.0F),
			Mx3DPointFloat(0.0F, 0.0F, 1.0F),
			Mx3DPointFloat(0.0F, 1.0F, 0.0F)
		);
		userActor->TransformPointOfView();
	}
}

MxBool OrbitCamera::HandleCameraRelativeMovement(
	LegoNavController* p_nav,
	const Vector3& p_curPos,
	const Vector3& p_curDir,
	Vector3& p_newPos,
	Vector3& p_newDir,
	float p_deltaTime,
	bool p_isInMultiPartEmote,
	bool p_lmbHeld
)
{
	LegoInputManager* inputManager = InputManager();
	MxU32 keyFlags = 0;
	if (!inputManager || inputManager->GetNavigationKeyStates(keyFlags) == FAILURE) {
		keyFlags = 0;
	}

	float camForwardX = -SDL_sinf(m_absoluteYaw);
	float camForwardZ = SDL_cosf(m_absoluteYaw);
	float camRightX = SDL_cosf(m_absoluteYaw);
	float camRightZ = SDL_sinf(m_absoluteYaw);

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
	if (p_lmbHeld) {
		moveDirX += camForwardX;
		moveDirZ += camForwardZ;
	}

	if (keyFlags == 0 && !p_lmbHeld && inputManager) {
		MxU32 joystickX, joystickY, povPosition;
		if (inputManager->GetJoystickState(&joystickX, &joystickY, &povPosition) == SUCCESS) {
			float jx = (joystickX - 50.0f) / 50.0f;
			float jy = -(joystickY - 50.0f) / 50.0f;

			if (SDL_fabsf(jx) < 0.1f) {
				jx = 0.0f;
			}
			if (SDL_fabsf(jy) < 0.1f) {
				jy = 0.0f;
			}

			moveDirX += camForwardX * jy + camRightX * jx;
			moveDirZ += camForwardZ * jy + camRightZ * jx;
		}
	}

	float moveDirLen = SDL_sqrtf(moveDirX * moveDirX + moveDirZ * moveDirZ);
	bool hasInput = moveDirLen > 0.001f;

	if (p_isInMultiPartEmote) {
		hasInput = false;
		m_smoothedSpeed = 0.0f;
	}

	if (hasInput) {
		moveDirX /= moveDirLen;
		moveDirZ /= moveDirLen;
	}

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
		p_newPos = p_curPos;
		p_newDir = p_curDir;
	}
	else {
		float speed = m_smoothedSpeed * p_deltaTime;
		if (hasInput) {
			p_newPos[0] = p_curPos[0] + moveDirX * speed;
			p_newPos[1] = p_curPos[1] + p_curDir[1] * speed;
			p_newPos[2] = p_curPos[2] + moveDirZ * speed;

			float targetYaw = SDL_atan2f(-moveDirX, moveDirZ);
			float currentYaw = SDL_atan2f(-p_curDir[0], p_curDir[2]);
			float angleDiff = targetYaw - currentYaw;

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
			p_newPos[0] = p_curPos[0] + p_curDir[0] * speed;
			p_newPos[1] = p_curPos[1] + p_curDir[1] * speed;
			p_newPos[2] = p_curPos[2] + p_curDir[2] * speed;
			p_newDir = p_curDir;
		}
	}

	p_nav->m_linearVel = m_smoothedSpeed;
	p_nav->m_rotationalVel = 0.0f;

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
