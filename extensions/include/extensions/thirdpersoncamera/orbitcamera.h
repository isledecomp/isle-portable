#pragma once

#include "mxgeometry/mxgeometry3d.h"
#include "mxtypes.h"

#include <SDL3/SDL_stdinc.h>

class LegoNavController;
class LegoPathActor;
class LegoROI;
class LegoWorld;
class Vector3;

namespace Extensions
{
namespace ThirdPersonCamera
{

class OrbitCamera {
public:
	OrbitCamera();

	void SetupCamera(LegoPathActor* p_actor);
	void ApplyOrbitCamera();
	void ResetOrbitState();
	void ClampPitch();
	void ClampDistance();
	void InitAbsoluteYaw(LegoROI* p_roi);

	void RestoreFirstPersonCamera();

	MxBool HandleCameraRelativeMovement(
		LegoNavController* p_nav,
		const Vector3& p_curPos,
		const Vector3& p_curDir,
		Vector3& p_newPos,
		Vector3& p_newDir,
		float p_deltaTime,
		bool p_isInMultiPartEmote
	);

	void AdjustYaw(float p_delta) { m_absoluteYaw += p_delta; }
	void AdjustPitch(float p_delta) { m_orbitPitch += p_delta; }
	void AdjustDistance(float p_delta) { m_orbitDistance += p_delta; }

	float GetOrbitDistance() const { return m_orbitDistance; }
	void SetOrbitDistance(float p_distance) { m_orbitDistance = p_distance; }
	float GetSmoothedSpeed() const { return m_smoothedSpeed; }

	static constexpr float DEFAULT_ORBIT_YAW = 0.0f;
	static constexpr float DEFAULT_ORBIT_PITCH = 0.3f;
	static constexpr float DEFAULT_ORBIT_DISTANCE = 3.5f;
	static constexpr float ORBIT_TARGET_HEIGHT = 1.5f;
	static constexpr float MIN_PITCH = 0.05f;
	static constexpr float MAX_PITCH = 1.4f;
	static constexpr float MIN_DISTANCE = 1.5f;
	static constexpr float SWITCH_TO_FIRST_PERSON_DISTANCE = 0.5f;
	static constexpr float MAX_DISTANCE = 15.0f;

private:
	void ComputeOrbitVectors(float p_yaw, Mx3DPointFloat& p_at, Mx3DPointFloat& p_dir, Mx3DPointFloat& p_up) const;
	float GetLocalYaw(LegoROI* p_roi) const;

	float m_orbitPitch;
	float m_orbitDistance;
	float m_absoluteYaw;
	float m_smoothedSpeed;
};

} // namespace ThirdPersonCamera
} // namespace Extensions
