#include "legoinputmanager.h"

#include "legocameracontroller.h"
#include "legocontrolmanager.h"
#include "legomain.h"
#include "legoutils.h"
#include "legovideomanager.h"
#include "legoworld.h"
#include "misc.h"
#include "mxautolock.h"
#include "mxdebug.h"
#include "roi/legoroi.h"

DECOMP_SIZE_ASSERT(LegoInputManager, 0x338)
DECOMP_SIZE_ASSERT(LegoNotifyList, 0x18)
DECOMP_SIZE_ASSERT(LegoNotifyListCursor, 0x10)
DECOMP_SIZE_ASSERT(LegoEventQueue, 0x18)

// GLOBAL: LEGO1 0x100f31b0
MxS32 g_unk0x100f31b0 = -1;

// GLOBAL: LEGO1 0x100f31b4
const char* g_unk0x100f31b4 = NULL;

// GLOBAL: LEGO1 0x100f67b8
MxBool g_unk0x100f67b8 = TRUE;

// FUNCTION: LEGO1 0x1005b790
LegoInputManager::LegoInputManager()
{
	m_keyboardNotifyList = NULL;
	m_world = NULL;
	m_camera = NULL;
	m_eventQueue = NULL;
	m_unk0x80 = FALSE;
	m_autoDragTimerID = 0;
	m_x = 0;
	m_y = 0;
	m_controlManager = NULL;
	m_unk0x81 = FALSE;
	m_unk0x88 = FALSE;
	m_unk0x195 = 0;
	m_joyids = NULL;
	m_joystickIndex = -1;
	m_joystick = NULL;
	m_useJoystick = FALSE;
	m_unk0x335 = FALSE;
	m_unk0x336 = FALSE;
	m_unk0x74 = 0x19;
	m_autoDragTime = 1000;
}

// FUNCTION: LEGO1 0x1005b8f0
LegoInputManager::~LegoInputManager()
{
	Destroy();
}

// FUNCTION: LEGO1 0x1005b960
MxResult LegoInputManager::Create(HWND p_hwnd)
{
	MxResult result = SUCCESS;

	m_controlManager = new LegoControlManager;

	if (!m_keyboardNotifyList) {
		m_keyboardNotifyList = new LegoNotifyList;
	}

	if (!m_eventQueue) {
		m_eventQueue = new LegoEventQueue;
	}

	GetJoystick();

	if (!m_keyboardNotifyList || !m_eventQueue) {
		Destroy();
		result = FAILURE;
	}

	return result;
}

// FUNCTION: LEGO1 0x1005bfe0
void LegoInputManager::Destroy()
{
	if (m_keyboardNotifyList) {
		delete m_keyboardNotifyList;
	}
	m_keyboardNotifyList = NULL;

	if (m_eventQueue) {
		delete m_eventQueue;
	}
	m_eventQueue = NULL;

	if (m_controlManager) {
		delete m_controlManager;
	}

	SDL_free(m_joyids);
}

// FUNCTION: LEGO1 0x1005c0f0
void LegoInputManager::GetKeyboardState()
{
	m_keyboardState = SDL_GetKeyboardState(NULL);
}

// FUNCTION: LEGO1 0x1005c160
MxResult LegoInputManager::GetNavigationKeyStates(MxU32& p_keyFlags)
{
	GetKeyboardState();

	if (g_unk0x100f67b8) {
		// [library:input] Figure out if we still need the logic that was here.
	}

	MxU32 keyFlags = 0;

	if (m_keyboardState[SDL_SCANCODE_KP_8] || m_keyboardState[SDL_SCANCODE_UP]) {
		keyFlags |= c_up;
	}

	if ((m_keyboardState[SDL_SCANCODE_KP_2] || m_keyboardState[SDL_SCANCODE_DOWN])) {
		keyFlags |= c_down;
	}

	if ((m_keyboardState[SDL_SCANCODE_KP_4] || m_keyboardState[SDL_SCANCODE_LEFT])) {
		keyFlags |= c_left;
	}

	if ((m_keyboardState[SDL_SCANCODE_KP_6] || m_keyboardState[SDL_SCANCODE_RIGHT])) {
		keyFlags |= c_right;
	}

	if (m_keyboardState[SDL_SCANCODE_LCTRL] || m_keyboardState[SDL_SCANCODE_RCTRL]) {
		keyFlags |= c_ctrl;
	}

	GetNavigationTouchStates(keyFlags);

	p_keyFlags = keyFlags;

	return SUCCESS;
}

// FUNCTION: LEGO1 0x1005c240
MxResult LegoInputManager::GetJoystick()
{
	if (m_joystick != NULL && SDL_GamepadConnected(m_joystick) == TRUE) {
		return SUCCESS;
	}

	MxS32 numJoysticks = 0;
	if (m_joyids != NULL) {
		SDL_free(m_joyids);
		m_joyids = NULL;
	}
	m_joyids = SDL_GetGamepads(&numJoysticks);

	if (m_useJoystick != FALSE && numJoysticks != 0) {
		MxS32 joyid = m_joystickIndex;
		if (joyid >= 0) {
			m_joystick = SDL_OpenGamepad(m_joyids[joyid]);
			if (m_joystick != NULL) {
				return SUCCESS;
			}
		}

		for (joyid = 0; joyid < numJoysticks; joyid++) {
			m_joystick = SDL_OpenGamepad(m_joyids[joyid]);
			if (m_joystick != NULL) {
				return SUCCESS;
			}
		}
	}

	return FAILURE;
}

// FUNCTION: LEGO1 0x1005c320
MxResult LegoInputManager::GetJoystickState(MxU32* p_joystickX, MxU32* p_joystickY, MxU32* p_povPosition)
{
	if (m_useJoystick != FALSE) {
		if (GetJoystick() == -1) {
			if (m_joystick != NULL) {
				// GetJoystick() failed but handle to joystick is still open, close it
				SDL_CloseGamepad(m_joystick);
				m_joystick = NULL;
			}

			return FAILURE;
		}

		MxS16 xPos = SDL_GetGamepadAxis(m_joystick, SDL_GAMEPAD_AXIS_LEFTX);
		MxS16 yPos = SDL_GetGamepadAxis(m_joystick, SDL_GAMEPAD_AXIS_LEFTY);
		if (xPos > -8000 && xPos < 8000) {
			// Ignore small axis values
			xPos = 0;
		}
		if (yPos > -8000 && yPos < 8000) {
			// Ignore small axis values
			yPos = 0;
		}

		// normalize values acquired from joystick axes
		*p_joystickX = ((xPos + 32768) * 100) / 65535;
		*p_joystickY = ((yPos + 32768) * 100) / 65535;

		return SUCCESS;
	}

	return FAILURE;
}

// FUNCTION: LEGO1 0x1005c470
void LegoInputManager::Register(MxCore* p_notify)
{
	AUTOLOCK(m_criticalSection);

	LegoNotifyListCursor cursor(m_keyboardNotifyList);
	if (!cursor.Find(p_notify)) {
		m_keyboardNotifyList->Append(p_notify);
	}
}

// FUNCTION: LEGO1 0x1005c5c0
void LegoInputManager::UnRegister(MxCore* p_notify)
{
	AUTOLOCK(m_criticalSection);

	LegoNotifyListCursor cursor(m_keyboardNotifyList);
	if (cursor.Find(p_notify)) {
		cursor.Detach();
	}
}

// FUNCTION: LEGO1 0x1005c700
void LegoInputManager::SetCamera(LegoCameraController* p_camera)
{
	m_camera = p_camera;
}

// FUNCTION: LEGO1 0x1005c710
void LegoInputManager::ClearCamera()
{
	m_camera = NULL;
}

// FUNCTION: LEGO1 0x1005c720
// FUNCTION: BETA10 0x100896b8
void LegoInputManager::SetWorld(LegoWorld* p_world)
{
	m_world = p_world;
}

// FUNCTION: LEGO1 0x1005c730
// FUNCTION: BETA10 0x100896dc
void LegoInputManager::ClearWorld()
{
	m_world = NULL;
}

// FUNCTION: LEGO1 0x1005c740
void LegoInputManager::QueueEvent(NotificationId p_id, MxU8 p_modifier, MxLong p_x, MxLong p_y, SDL_Keycode p_key)
{
	LegoEventNotificationParam param = LegoEventNotificationParam(p_id, NULL, p_modifier, p_x, p_y, p_key);

	if (((!m_unk0x88) || ((m_unk0x335 && (param.GetNotification() == c_notificationButtonDown)))) ||
		((m_unk0x336 && (p_key == SDLK_SPACE)))) {
		ProcessOneEvent(param);
	}
}

// FUNCTION: LEGO1 0x1005c820
void LegoInputManager::ProcessEvents()
{
	AUTOLOCK(m_criticalSection);

	LegoEventNotificationParam event;
	while (m_eventQueue->Dequeue(event)) {
		if (ProcessOneEvent(event)) {
			break;
		}
	}
}

// FUNCTION: LEGO1 0x1005c9c0
MxBool LegoInputManager::ProcessOneEvent(LegoEventNotificationParam& p_param)
{
	MxBool processRoi;

	if (p_param.GetNotification() == c_notificationKeyPress) {
		if (!Lego()->IsPaused() || p_param.GetKey() == SDLK_PAUSE) {
			if (p_param.GetKey() == SDLK_LSHIFT || p_param.GetKey() == SDLK_RSHIFT) {
				if (m_unk0x195) {
					m_unk0x80 = FALSE;
					p_param.SetNotification(c_notificationDragEnd);

					if (m_camera) {
						m_camera->Notify(p_param);
					}
				}

				m_unk0x195 = !m_unk0x195;
				return TRUE;
			}

			LegoNotifyListCursor cursor(m_keyboardNotifyList);
			MxCore* target;

			while (cursor.Next(target)) {
				if (target->Notify(p_param) != 0) {
					return TRUE;
				}
			}
		}
	}
	else {
		if (!Lego()->IsPaused()) {
			processRoi = TRUE;

			if (m_unk0x335 != 0) {
				if (p_param.GetNotification() == c_notificationButtonDown) {
					LegoEventNotificationParam notification(c_notificationKeyPress, NULL, 0, 0, 0, SDLK_SPACE);
					LegoNotifyListCursor cursor(m_keyboardNotifyList);
					MxCore* target;

					while (cursor.Next(target)) {
						if (target->Notify(notification) != 0) {
							return TRUE;
						}
					}
				}

				return TRUE;
			}

			if (m_unk0x195 && p_param.GetNotification() == c_notificationButtonDown) {
				m_unk0x195 = 0;
				return TRUE;
			}

			if (m_world != NULL && m_world->Notify(p_param) != 0) {
				return TRUE;
			}

			if (p_param.GetNotification() == c_notificationButtonDown) {
				MxPresenter* presenter = VideoManager()->GetPresenterAt(p_param.GetX(), p_param.GetY());

				if (presenter) {
					if (presenter->GetDisplayZ() < 0) {
						processRoi = FALSE;

						if (m_controlManager->FUN_10029210(p_param, presenter)) {
							return TRUE;
						}
					}
					else {
						LegoROI* roi = PickROI(p_param.GetX(), p_param.GetY());

						if (roi == NULL && m_controlManager->FUN_10029210(p_param, presenter)) {
							return TRUE;
						}
					}
				}
			}
			else if (p_param.GetNotification() == c_notificationButtonUp) {
				if (g_unk0x100f31b0 != -1 || m_controlManager->GetUnknown0x10() ||
					m_controlManager->GetUnknown0x0c() == 1) {
					MxBool result = m_controlManager->FUN_10029210(p_param, NULL);
					StopAutoDragTimer();

					m_unk0x80 = FALSE;
					m_unk0x81 = FALSE;
					return result;
				}
			}

			if (FUN_1005cdf0(p_param)) {
				if (processRoi && p_param.GetNotification() == c_notificationClick) {
					LegoROI* roi = PickROI(p_param.GetX(), p_param.GetY());
					p_param.SetROI(roi);

					if (roi && roi->GetVisibility() == TRUE) {
						for (OrientableROI* parent = roi->GetParentROI(); parent; parent = parent->GetParentROI()) {
							roi = (LegoROI*) parent;
						}

						LegoEntity* entity = roi->GetEntity();
						if (entity && entity->Notify(p_param) != 0) {
							return TRUE;
						}
					}
				}

				if (m_camera && m_camera->Notify(p_param) != 0) {
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}

// FUNCTION: LEGO1 0x1005cdf0
// FUNCTION: BETA10 0x10089cc1
MxBool LegoInputManager::FUN_1005cdf0(LegoEventNotificationParam& p_param)
{
	MxBool result = FALSE;

	switch (p_param.GetNotification()) {
	case c_notificationButtonDown:
		m_x = p_param.GetX();
		m_y = p_param.GetY();
		m_unk0x80 = FALSE;
		m_unk0x81 = TRUE;
		StartAutoDragTimer();
		break;
	case c_notificationButtonUp:
		StopAutoDragTimer();

		if (m_unk0x80) {
			p_param.SetNotification(c_notificationDragEnd);
			result = TRUE;
		}
		else if (m_unk0x81) {
			p_param.SetX(m_x);
			p_param.SetY(m_y);
			p_param.SetNotification(c_notificationClick);
			result = TRUE;
		}

		m_unk0x80 = FALSE;
		m_unk0x81 = FALSE;
		break;
	case c_notificationMouseMove:
		if (m_unk0x195) {
			p_param.SetModifier(LegoEventNotificationParam::c_lButtonState);
		}

		if ((m_unk0x195 || m_unk0x81) && p_param.GetModifier() & LegoEventNotificationParam::c_lButtonState) {
			if (!m_unk0x80) {
				if (m_unk0x195) {
					m_x = p_param.GetX();
					m_y = p_param.GetY();
				}

				MxS32 diffX = p_param.GetX() - m_x;
				MxS32 diffY = p_param.GetY() - m_y;
				MxS32 t = diffX * diffX + diffY * diffY;

				if (m_unk0x195 || t > m_unk0x74) {
					StopAutoDragTimer();
					m_unk0x80 = TRUE;
					p_param.SetNotification(c_notificationDragStart);
					result = TRUE;
					p_param.SetX(m_x);
					p_param.SetY(m_y);
				}
			}
			else {
				p_param.SetNotification(c_notificationDrag);
				result = TRUE;
			}
		}
		break;
	case c_notificationTimer:
		if (p_param.GetModifier() == m_autoDragTimerID) {
			StopAutoDragTimer();

			if (m_unk0x81) {
				m_unk0x80 = TRUE;
				p_param.SetX(m_x);
				p_param.SetY(m_y);
				p_param.SetModifier(LegoEventNotificationParam::c_lButtonState);
				p_param.SetNotification(c_notificationDragStart);
				result = TRUE;
			}
			else {
				m_unk0x80 = FALSE;
			}
		}
		break;
	}

	return result;
}

static Uint32 SDLCALL LegoInputManagerTimerCallback(void* userdata, SDL_TimerID timerID, Uint32 interval)
{
	LegoInputManager* inputManager = (LegoInputManager*) userdata;
	SDL_Event event;
	event.type = g_legoSdlEvents.m_windowsMessage;
	event.user.code = WM_TIMER;
	event.user.data1 = (void*) (uintptr_t) timerID;
	event.user.data2 = NULL;
	return interval;
}

// FUNCTION: LEGO1 0x1005cfb0
// FUNCTION: BETA10 0x10089fc5
void LegoInputManager::StartAutoDragTimer()
{
	m_autoDragTimerID = SDL_AddTimer(m_autoDragTime, LegoInputManagerTimerCallback, this);
}

// FUNCTION: LEGO1 0x1005cfd0
// FUNCTION: BETA10 0x1008a005
void LegoInputManager::StopAutoDragTimer()
{
	if (m_autoDragTimerID) {
		SDL_RemoveTimer(m_autoDragTimerID);
		m_autoDragTimerID = 0;
	}
}

// FUNCTION: LEGO1 0x1005cff0
void LegoInputManager::EnableInputProcessing()
{
	m_unk0x88 = FALSE;
	g_unk0x100f31b0 = -1;
	g_unk0x100f31b4 = NULL;
}

MxResult LegoInputManager::GetNavigationTouchStates(MxU32& p_keyStates)
{
	int count;
	SDL_TouchID* touchDevices = SDL_GetTouchDevices(&count);

	if (touchDevices) {
		auto applyFingerNavigation = [&p_keyStates](SDL_TouchID p_touchId) {
			int count;
			SDL_Finger** fingers = SDL_GetTouchFingers(p_touchId, &count);

			if (fingers) {
				for (int i = 0; i < count; i++) {
					if (fingers[i]->y > 3.0 / 4.0) {
						if (fingers[i]->x < 1.0 / 3.0) {
							p_keyStates |= c_left;
						}
						else if (fingers[i]->x > 2.0 / 3.0) {
							p_keyStates |= c_right;
						}
						else {
							p_keyStates |= c_down;
						}
					}
					else {
						p_keyStates |= c_up;
					}
				}

				SDL_free(fingers);
			}
		};

		for (int i = 0; i < count; i++) {
			applyFingerNavigation(touchDevices[i]);
		}

		SDL_free(touchDevices);
	}

	return SUCCESS;
}
