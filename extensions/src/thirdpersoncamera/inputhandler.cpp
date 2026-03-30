#include "extensions/thirdpersoncamera/inputhandler.h"

#include "extensions/thirdpersoncamera/orbitcamera.h"

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <utility>

using namespace Extensions::ThirdPersonCamera;

InputHandler::InputHandler()
	: m_touch{}, m_wantsAutoDisable(false), m_wantsAutoEnable(false), m_rightButtonHeld(false), m_leftButtonHeld(false),
	  m_leftButtonDownTime(0), m_savedMouseX(0.0f), m_savedMouseY(0.0f)
{
}

bool InputHandler::TryClaimFinger(const SDL_TouchFingerEvent& p_event)
{
	if (m_touch.count >= 2 || p_event.x < CAMERA_ZONE_X || IsFingerTracked(p_event.fingerID)) {
		return false;
	}

	int idx = m_touch.count;
	m_touch.id[idx] = p_event.fingerID;
	m_touch.x[idx] = p_event.x;
	m_touch.y[idx] = p_event.y;
	m_touch.synced[idx] = true;
	m_touch.count++;

	if (m_touch.count == 2) {
		float dx = m_touch.x[1] - m_touch.x[0];
		float dy = m_touch.y[1] - m_touch.y[0];
		m_touch.initialPinchDist = SDL_sqrtf(dx * dx + dy * dy);
		m_touch.gesturePinchDist = m_touch.initialPinchDist;
	}

	return true;
}

bool InputHandler::TryReleaseFinger(SDL_FingerID p_id)
{
	for (int i = 0; i < m_touch.count; i++) {
		if (m_touch.id[i] == p_id) {
			if (i == 0 && m_touch.count == 2) {
				m_touch.id[0] = m_touch.id[1];
				m_touch.x[0] = m_touch.x[1];
				m_touch.y[0] = m_touch.y[1];
				m_touch.synced[0] = m_touch.synced[1];
			}
			m_touch.count--;
			m_touch.initialPinchDist = 0.0f;
			m_touch.gesturePinchDist = 0.0f;
			return true;
		}
	}
	return false;
}

bool InputHandler::IsFingerTracked(SDL_FingerID p_id) const
{
	for (int i = 0; i < m_touch.count; i++) {
		if (m_touch.id[i] == p_id) {
			return true;
		}
	}
	return false;
}

bool InputHandler::ConsumeAutoDisable()
{
	return std::exchange(m_wantsAutoDisable, false);
}

bool InputHandler::ConsumeAutoEnable()
{
	return std::exchange(m_wantsAutoEnable, false);
}

bool InputHandler::IsLmbHeldForMovement() const
{
	return m_leftButtonHeld && m_leftButtonDownTime > 0 &&
		   (m_rightButtonHeld || (SDL_GetTicks() - m_leftButtonDownTime) >= LMB_HOLD_THRESHOLD_MS);
}

void InputHandler::SuppressGestures()
{
	m_touch.synced[0] = false;
	m_touch.synced[1] = false;
	m_touch.initialPinchDist = 0.0f;
	m_touch.gesturePinchDist = 0.0f;
}

void InputHandler::HandleSDLEvent(SDL_Event* p_event, OrbitCamera& p_orbit, bool p_active)
{
	switch (p_event->type) {
	case SDL_EVENT_MOUSE_WHEEL:
		if (!p_active) {
			if (p_event->wheel.y < 0) {
				m_wantsAutoEnable = true;
			}
			break;
		}
		if (p_orbit.GetOrbitDistance() <= OrbitCamera::SWITCH_TO_FIRST_PERSON_DISTANCE && p_event->wheel.y > 0) {
			m_wantsAutoDisable = true;
			break;
		}
		p_orbit.AdjustDistance(-p_event->wheel.y * MOUSE_WHEEL_ZOOM_STEP);
		p_orbit.ClampDistance();
		break;

	case SDL_EVENT_MOUSE_MOTION:
		if (!p_active) {
			break;
		}
		if (m_rightButtonHeld) {
			p_orbit.AdjustYaw(-p_event->motion.xrel * MOUSE_SENSITIVITY);
			p_orbit.AdjustPitch(p_event->motion.yrel * MOUSE_SENSITIVITY);
			p_orbit.ClampPitch();
		}
		break;

	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP: {
		if (p_event->button.button == SDL_BUTTON_RIGHT) {
			m_rightButtonHeld = p_event->button.down;
			SDL_Window* window = SDL_GetWindowFromID(p_event->button.windowID);
			if (window) {
				if (m_rightButtonHeld) {
					if (p_active) {
						SDL_GetMouseState(&m_savedMouseX, &m_savedMouseY);
						SDL_SetWindowRelativeMouseMode(window, true);
					}
				}
				else if (SDL_GetWindowRelativeMouseMode(window)) {
					SDL_SetWindowRelativeMouseMode(window, false);
					SDL_WarpMouseInWindow(window, m_savedMouseX, m_savedMouseY);
				}
			}
		}
		else if (p_event->button.button == SDL_BUTTON_LEFT) {
			m_leftButtonHeld = p_event->button.down;
			m_leftButtonDownTime = p_event->button.down ? SDL_GetTicks() : 0;
		}
		break;
	}

	case SDL_EVENT_FINGER_DOWN:
		TryClaimFinger(p_event->tfinger);
		break;

	case SDL_EVENT_FINGER_MOTION: {
		if (m_touch.count == 1) {
			if (!p_active) {
				break;
			}
			if (m_touch.id[0] == p_event->tfinger.fingerID) {
				if (!m_touch.synced[0]) {
					m_touch.x[0] = p_event->tfinger.x;
					m_touch.y[0] = p_event->tfinger.y;
					m_touch.synced[0] = true;
					break;
				}

				float oldX = m_touch.x[0];
				float oldY = m_touch.y[0];
				m_touch.x[0] = p_event->tfinger.x;
				m_touch.y[0] = p_event->tfinger.y;

				float moveX = m_touch.x[0] - oldX;
				float moveY = m_touch.y[0] - oldY;
				p_orbit.AdjustYaw(-moveX * TOUCH_YAW_PITCH_SCALE);
				p_orbit.AdjustPitch(moveY * TOUCH_YAW_PITCH_SCALE);
				p_orbit.ClampPitch();
			}
		}
		else if (m_touch.count == 2) {
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

			if (!m_touch.synced[idx]) {
				m_touch.x[idx] = p_event->tfinger.x;
				m_touch.y[idx] = p_event->tfinger.y;
				m_touch.synced[idx] = true;

				if (m_touch.synced[0] && m_touch.synced[1]) {
					float dx = m_touch.x[1] - m_touch.x[0];
					float dy = m_touch.y[1] - m_touch.y[0];
					m_touch.initialPinchDist = SDL_sqrtf(dx * dx + dy * dy);
					m_touch.gesturePinchDist = m_touch.initialPinchDist;
				}
				break;
			}

			float oldX = m_touch.x[idx];
			float oldY = m_touch.y[idx];
			m_touch.x[idx] = p_event->tfinger.x;
			m_touch.y[idx] = p_event->tfinger.y;

			float dx = m_touch.x[1] - m_touch.x[0];
			float dy = m_touch.y[1] - m_touch.y[0];
			float newDist = SDL_sqrtf(dx * dx + dy * dy);

			if (m_touch.initialPinchDist > 0.001f) {
				float pinchDelta = m_touch.initialPinchDist - newDist;

				if (!p_active) {
					float totalDelta = m_touch.gesturePinchDist - newDist;
					if (totalDelta > PINCH_TRANSITION_THRESHOLD) {
						m_wantsAutoEnable = true;
						m_touch.gesturePinchDist = newDist;
					}
					m_touch.initialPinchDist = newDist;
					break;
				}

				if (p_orbit.GetOrbitDistance() <= OrbitCamera::SWITCH_TO_FIRST_PERSON_DISTANCE) {
					float totalDelta = newDist - m_touch.gesturePinchDist;
					if (totalDelta > PINCH_TRANSITION_THRESHOLD) {
						m_wantsAutoDisable = true;
						m_touch.initialPinchDist = newDist;
						m_touch.gesturePinchDist = newDist;
						break;
					}
				}

				p_orbit.AdjustDistance(pinchDelta * PINCH_ZOOM_SCALE);
				p_orbit.ClampDistance();
				m_touch.initialPinchDist = newDist;
			}

			float moveX = m_touch.x[idx] - oldX;
			float moveY = m_touch.y[idx] - oldY;
			p_orbit.AdjustYaw(-moveX * TOUCH_YAW_PITCH_SCALE);
			p_orbit.AdjustPitch(moveY * TOUCH_YAW_PITCH_SCALE);
			p_orbit.ClampPitch();
		}
		break;
	}

	case SDL_EVENT_FINGER_UP:
	case SDL_EVENT_FINGER_CANCELED: {
		TryReleaseFinger(p_event->tfinger.fingerID);
		break;
	}

	default:
		break;
	}
}
