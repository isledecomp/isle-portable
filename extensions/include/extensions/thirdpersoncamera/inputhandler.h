#pragma once

#include <SDL3/SDL_events.h>

namespace Extensions
{
namespace ThirdPersonCamera
{

class OrbitCamera;

class InputHandler {
public:
	InputHandler();

	void HandleSDLEvent(SDL_Event* p_event, OrbitCamera& p_orbit, bool p_active);

	bool TryClaimFinger(const SDL_TouchFingerEvent& p_event, bool p_active);
	bool TryReleaseFinger(SDL_FingerID p_id);
	bool IsFingerTracked(SDL_FingerID p_id) const;

	bool ConsumeAutoDisable();
	bool ConsumeAutoEnable();

	void ResetTouchState() { m_touch = {}; }

	static constexpr float CAMERA_ZONE_X = 0.5f;

private:
	struct TouchState {
		SDL_FingerID id[2];
		float x[2], y[2];
		int count;
		float initialPinchDist;
	} m_touch;

	bool m_wantsAutoDisable;
	bool m_wantsAutoEnable;
};

} // namespace ThirdPersonCamera
} // namespace Extensions
