#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>

namespace Extensions
{
namespace ThirdPersonCamera
{

class OrbitCamera;

class InputHandler {
public:
	InputHandler();

	void HandleSDLEvent(SDL_Event* p_event, OrbitCamera& p_orbit, bool p_active, bool p_mouseLookEngaged);

	bool TryClaimFinger(const SDL_TouchFingerEvent& p_event);
	bool TryReleaseFinger(SDL_FingerID p_id);
	bool IsFingerTracked(SDL_FingerID p_id) const;
	int GetTouchCount() const { return m_touch.count; }
	SDL_FingerID GetFingerID(int p_idx) const { return m_touch.id[p_idx]; }

	bool IsLeftButtonHeld() const { return m_leftButtonHeld; }
	bool IsLmbHeldForMovement() const;

	bool ConsumeAutoDisable();
	bool ConsumeAutoEnable();

	void ResetTouchState() { m_touch = {}; }
	void SuppressGestures();

	static constexpr float CAMERA_ZONE_X = 0.5f;
	static constexpr float PINCH_TRANSITION_THRESHOLD = 0.03f;
	static constexpr Uint64 LMB_HOLD_THRESHOLD_MS = 300;
	static constexpr float MOUSE_SENSITIVITY = 0.005f;
	static constexpr float MOUSE_WHEEL_ZOOM_STEP = 0.5f;
	static constexpr float TOUCH_YAW_PITCH_SCALE = 2.0f;
	static constexpr float PINCH_ZOOM_SCALE = 6.0f;

	static float s_mouseSensitivityX;
	static float s_mouseSensitivityY;
	static bool s_invertY;
	static bool s_alwaysOnMouseLook;
	static SDL_Keycode s_toggleKey;
	static int s_toggleMouseButton;

private:
	struct TouchState {
		SDL_FingerID id[2];
		float x[2], y[2];
		bool synced[2];
		int count;
		float initialPinchDist;
		float gesturePinchDist;
	} m_touch;

	bool m_wantsAutoDisable;
	bool m_wantsAutoEnable;
	bool m_rightButtonHeld;
	bool m_leftButtonHeld;
	Uint64 m_leftButtonDownTime;
	// Saved cursor position for the original hold-RMB look mode, so the cursor
	// can be restored where it was when the button is released.
	float m_savedMouseX;
	float m_savedMouseY;
};

} // namespace ThirdPersonCamera
} // namespace Extensions
