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

// FUNCTION: LEGO1 0x1005b8b0
MxResult LegoInputManager::Tickle()
{
	ProcessEvents();
	return SUCCESS;
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
		keyFlags |= c_bit5;
	}

	p_keyFlags = keyFlags;

	return SUCCESS;
}

// FUNCTION: LEGO1 0x1005c240
MxResult LegoInputManager::GetJoystick()
{
	if (m_joystick != NULL && SDL_JoystickConnected(m_joystick) == TRUE) {
		return SUCCESS;
	}

	MxS32 numJoysticks = 0;
	m_joyids = SDL_GetJoysticks(&numJoysticks);

	if (m_useJoystick != FALSE && numJoysticks != 0) {
		MxS32 joyid = m_joystickIndex;
		if (joyid >= 0) {
			m_joystick = SDL_OpenJoystick(m_joyids[joyid]);
			if (m_joystick != NULL) {
				return SUCCESS;
			}
		}

		for (joyid = 0; joyid < numJoysticks; joyid++) {
			m_joystick = SDL_OpenJoystick(m_joyids[joyid]);
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
				SDL_CloseJoystick(m_joystick);
				m_joystick = NULL;
			}

			return FAILURE;
		}

		MxS16 xPos = SDL_GetJoystickAxis(m_joystick, 0);
		MxS16 yPos = SDL_GetJoystickAxis(m_joystick, 1);
		MxU8 hatPos = SDL_GetJoystickHat(m_joystick, 0);

		// normalize values acquired from joystick axes
		*p_joystickX = ((xPos + 32768) * 100) / 65535;
		*p_joystickY = ((yPos + 32768) * 100) / 65535;

		switch (hatPos) {
		case SDL_HAT_CENTERED:
			*p_povPosition = (MxU32) -1;
			break;
		case SDL_HAT_UP:
			*p_povPosition = (MxU32) 0;
			break;
		case SDL_HAT_RIGHT:
			*p_povPosition = (MxU32) 9000 / 100;
			break;
		case SDL_HAT_DOWN:
			*p_povPosition = (MxU32) 18000 / 100;
			break;
		case SDL_HAT_LEFT:
			*p_povPosition = (MxU32) 27000 / 100;
			break;
		case SDL_HAT_RIGHTUP:
			*p_povPosition = (MxU32) 4500 / 100;
			break;
		case SDL_HAT_RIGHTDOWN:
			*p_povPosition = (MxU32) 13500 / 100;
			break;
		case SDL_HAT_LEFTUP:
			*p_povPosition = (MxU32) 31500 / 100;
			break;
		case SDL_HAT_LEFTDOWN:
			*p_povPosition = (MxU32) 22500 / 100;
			break;
		default:
			*p_povPosition = (MxU32) -1;
			break;
		}

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
void LegoInputManager::QueueEvent(NotificationId p_id, MxU8 p_modifier, MxLong p_x, MxLong p_y, MxU8 p_key)
{
	LegoEventNotificationParam param = LegoEventNotificationParam(p_id, NULL, p_modifier, p_x, p_y, p_key);

	if (((!m_unk0x88) || ((m_unk0x335 && (param.GetNotification() == c_notificationButtonDown)))) ||
		((m_unk0x336 && (p_key == VK_SPACE)))) {
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
		if (!Lego()->IsPaused() || p_param.GetKey() == VK_PAUSE) {
			if (p_param.GetKey() == VK_SHIFT) {
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
					LegoEventNotificationParam notification(c_notificationKeyPress, NULL, 0, 0, 0, VK_SPACE);
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

					if (roi && roi->GetVisibility()) {
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

// FUNCTION: LEGO1 0x1005cfb0
// FUNCTION: BETA10 0x10089fc5
void LegoInputManager::StartAutoDragTimer()
{
	m_autoDragTimerID = ::SetTimer(LegoOmni::GetInstance()->GetWindowHandle(), 1, m_autoDragTime, NULL);
}

// FUNCTION: LEGO1 0x1005cfd0
// FUNCTION: BETA10 0x1008a005
void LegoInputManager::StopAutoDragTimer()
{
	if (m_autoDragTimerID) {
		::KillTimer(LegoOmni::GetInstance()->GetWindowHandle(), m_autoDragTimerID);
	}
}

// FUNCTION: LEGO1 0x1005cff0
void LegoInputManager::EnableInputProcessing()
{
	m_unk0x88 = FALSE;
	g_unk0x100f31b0 = -1;
	g_unk0x100f31b4 = NULL;
}
