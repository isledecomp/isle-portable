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

#include <SDL3/SDL_log.h>

#ifdef __WIIU__
template <class... Ts>
struct overloaded : Ts... {
	using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
#endif

DECOMP_SIZE_ASSERT(LegoInputManager, 0x338)
DECOMP_SIZE_ASSERT(LegoNotifyList, 0x18)
DECOMP_SIZE_ASSERT(LegoNotifyListCursor, 0x10)
DECOMP_SIZE_ASSERT(LegoEventQueue, 0x18)

// GLOBAL: LEGO1 0x100f31b0
MxS32 g_clickedObjectId = -1;

// GLOBAL: LEGO1 0x100f31b4
const char* g_clickedAtom = NULL;

// GLOBAL: LEGO1 0x100f67b8
MxBool g_unk0x100f67b8 = TRUE;

// FUNCTION: LEGO1 0x1005b790
// STUB: BETA10 0x10088a8e
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
// STUB: BETA10 0x10088c9c
MxResult LegoInputManager::Create(HWND p_hwnd)
{
	MxResult result = SUCCESS;

	m_controlManager = new LegoControlManager;
	assert(m_controlManager);

	if (!m_keyboardNotifyList) {
		m_keyboardNotifyList = new LegoNotifyList;
	}

	if (!m_eventQueue) {
		m_eventQueue = new LegoEventQueue;
	}

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

	for (const auto& [id, joystick] : m_joysticks) {
		if (joystick.second) {
			SDL_CloseHaptic(joystick.second);
		}

		SDL_CloseGamepad(joystick.first);
	}

	for (const auto& [id, mouse] : m_mice) {
		if (mouse.second) {
			SDL_CloseHaptic(mouse.second);
		}
	}

	for (const auto& [id, haptic] : m_otherHaptics) {
		SDL_CloseHaptic(haptic);
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
		keyFlags |= c_ctrl;
	}

	GetNavigationTouchStates(keyFlags);

	p_keyFlags = keyFlags;

	return SUCCESS;
}

// FUNCTION: LEGO1 0x1005c320
MxResult LegoInputManager::GetJoystickState(MxU32* p_joystickX, MxU32* p_joystickY, MxU32* p_povPosition)
{
	if (!std::holds_alternative<SDL_JoystickID_v>(m_lastInputMethod) &&
		!(std::holds_alternative<SDL_TouchID_v>(m_lastInputMethod) && m_touchScheme == e_gamepad)) {
		return FAILURE;
	}

	MxS16 xPos, yPos = 0;
	for (const auto& [id, joystick] : m_joysticks) {
		xPos = SDL_GetGamepadAxis(joystick.first, SDL_GAMEPAD_AXIS_LEFTX);
		yPos = SDL_GetGamepadAxis(joystick.first, SDL_GAMEPAD_AXIS_LEFTY);
		if (xPos > -8000 && xPos < 8000) {
			xPos = 0;
		}
		if (yPos > -8000 && yPos < 8000) {
			yPos = 0;
		}

		if (xPos || yPos) {
			break;
		}
	}

	if (!xPos && !yPos) {
		xPos = m_touchVirtualThumb.x;
		yPos = m_touchVirtualThumb.y;
	}

	*p_joystickX = ((xPos + 32768) * 100) / 65535;
	*p_joystickY = ((yPos + 32768) * 100) / 65535;
	*p_povPosition = -1;
	return SUCCESS;
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

						if (m_controlManager->HandleButtonDown(p_param, presenter)) {
							return TRUE;
						}
					}
					else {
						LegoROI* roi = PickROI(p_param.GetX(), p_param.GetY());

						if (roi == NULL && m_controlManager->HandleButtonDown(p_param, presenter)) {
							return TRUE;
						}
					}
				}
			}
			else if (p_param.GetNotification() == c_notificationButtonUp) {
				if (g_clickedObjectId != -1 || m_controlManager->IsSecondButtonDown() ||
					m_controlManager->HandleUpNextTickle() == 1) {
					MxBool result = m_controlManager->HandleButtonDown(p_param, NULL);
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
	g_clickedObjectId = -1;
	g_clickedAtom = NULL;
}

MxResult LegoInputManager::GetNavigationTouchStates(MxU32& p_keyStates)
{
	if (m_touchScheme == e_arrowKeys) {
		for (auto& [fingerID, touchFlags] : m_touchFlags) {
			p_keyStates |= touchFlags;
		}
	}

	return SUCCESS;
}

void LegoInputManager::AddKeyboard(SDL_KeyboardID p_keyboardID)
{
	if (m_keyboards.count(p_keyboardID)) {
		return;
	}

	m_keyboards[p_keyboardID] = {nullptr, nullptr};
}

void LegoInputManager::RemoveKeyboard(SDL_KeyboardID p_keyboardID)
{
	if (!m_keyboards.count(p_keyboardID)) {
		return;
	}

	m_keyboards.erase(p_keyboardID);
}

void LegoInputManager::AddMouse(SDL_MouseID p_mouseID)
{
	if (m_mice.count(p_mouseID)) {
		return;
	}

	// Currently no way to get an individual haptic device for a mouse.
	SDL_Haptic* haptic = SDL_OpenHapticFromMouse();
	if (haptic) {
		if (!SDL_InitHapticRumble(haptic)) {
			SDL_CloseHaptic(haptic);
			haptic = nullptr;
		}
	}

	m_mice[p_mouseID] = {nullptr, haptic};
}

void LegoInputManager::RemoveMouse(SDL_MouseID p_mouseID)
{
	if (!m_mice.count(p_mouseID)) {
		return;
	}

	if (m_mice[p_mouseID].second) {
		SDL_CloseHaptic(m_mice[p_mouseID].second);
	}

	m_mice.erase(p_mouseID);
}

void LegoInputManager::AddJoystick(SDL_JoystickID p_joystickID)
{
	if (m_joysticks.count(p_joystickID)) {
		return;
	}

	SDL_Gamepad* joystick = SDL_OpenGamepad(p_joystickID);
	if (joystick) {
		SDL_Haptic* haptic = SDL_OpenHapticFromJoystick(SDL_GetGamepadJoystick(joystick));
		if (haptic) {
			if (!SDL_InitHapticRumble(haptic)) {
				SDL_CloseHaptic(haptic);
				haptic = nullptr;
			}
		}

		m_joysticks[p_joystickID] = {joystick, haptic};
	}
	else {
		SDL_Log("Failed to open gamepad: %s", SDL_GetError());
	}
}

void LegoInputManager::RemoveJoystick(SDL_JoystickID p_joystickID)
{
	if (!m_joysticks.count(p_joystickID)) {
		return;
	}

	if (m_joysticks[p_joystickID].second) {
		SDL_CloseHaptic(m_joysticks[p_joystickID].second);
	}

	SDL_CloseGamepad(m_joysticks[p_joystickID].first);
	m_joysticks.erase(p_joystickID);
}

MxBool LegoInputManager::HandleTouchEvent(SDL_Event* p_event, TouchScheme p_touchScheme)
{
	const SDL_TouchFingerEvent& event = p_event->tfinger;
	m_touchScheme = p_touchScheme;

	switch (p_touchScheme) {
	case e_mouse:
		// Handled in LegoCameraController
		return FALSE;
	case e_arrowKeys:
		switch (p_event->type) {
		case SDL_EVENT_FINGER_UP:
		case SDL_EVENT_FINGER_CANCELED:
			m_touchFlags.erase(event.fingerID);
			break;
		case SDL_EVENT_FINGER_DOWN:
		case SDL_EVENT_FINGER_MOTION:
			m_touchFlags[event.fingerID] = 0;

			if (event.y > 3.0 / 4.0) {
				if (event.x < 1.0 / 3.0) {
					m_touchFlags[event.fingerID] |= c_left;
				}
				else if (event.x > 2.0 / 3.0) {
					m_touchFlags[event.fingerID] |= c_right;
				}
				else {
					m_touchFlags[event.fingerID] |= c_down;
				}
			}
			else {
				m_touchFlags[event.fingerID] |= c_up;
			}
			break;
		}
		break;
	case e_gamepad: {
		static SDL_FingerID g_finger = (SDL_FingerID) 0;

		switch (p_event->type) {
		case SDL_EVENT_FINGER_DOWN:
			if (!g_finger) {
				g_finger = event.fingerID;
				m_touchVirtualThumb = {0, 0};
				m_touchVirtualThumbOrigin = {event.x, event.y};
			}
			break;
		case SDL_EVENT_FINGER_UP:
		case SDL_EVENT_FINGER_CANCELED:
			if (event.fingerID == g_finger) {
				g_finger = 0;
				m_touchVirtualThumb = {0, 0};
				m_touchVirtualThumbOrigin = {0, 0};
			}
			break;
		case SDL_EVENT_FINGER_MOTION:
			if (event.fingerID == g_finger) {
				const float thumbstickRadius = 0.25f;
				const float deltaX =
					SDL_clamp(event.x - m_touchVirtualThumbOrigin.x, -thumbstickRadius, thumbstickRadius);
				const float deltaY =
					SDL_clamp(event.y - m_touchVirtualThumbOrigin.y, -thumbstickRadius, thumbstickRadius);

				m_touchVirtualThumb = {
					(int) (deltaX / thumbstickRadius * 32767.0f),
					(int) (deltaY / thumbstickRadius * 32767.0f),
				};
			}
			break;
		}
		break;
	}
	}

	return TRUE;
}

MxBool LegoInputManager::HandleRumbleEvent(
	float p_strength,
	float p_lowFrequencyRumble,
	float p_highFrequencyRumble,
	MxU32 p_milliseconds
)
{
	static bool g_hapticsInitialized = false;

	if (!g_hapticsInitialized) {
		InitializeHaptics();
		g_hapticsInitialized = true;
	}

	SDL_Haptic* haptic = nullptr;
	std::visit(
		overloaded{
			[](SDL_KeyboardID_v p_id) {},
			[&haptic, this](SDL_MouseID_v p_id) {
				if (m_mice.count((SDL_MouseID) p_id)) {
					haptic = m_mice[(SDL_MouseID) p_id].second;
				}
			},
			[&haptic, this](SDL_JoystickID_v p_id) {
				if (m_joysticks.count((SDL_JoystickID) p_id)) {
					haptic = m_joysticks[(SDL_JoystickID) p_id].second;
				}
			},
			[&haptic, this](SDL_TouchID_v p_id) {
				// We can't currently correlate Touch devices with Haptic devices
				if (!m_otherHaptics.empty()) {
					haptic = m_otherHaptics.begin()->second;
				}
			}
		},
		m_lastInputMethod
	);

	if (haptic) {
		return SDL_PlayHapticRumble(haptic, p_strength, p_milliseconds);
	}

	// A joystick isn't necessarily a haptic device; try basic rumble instead
	if (const SDL_JoystickID_v* joystick = std::get_if<SDL_JoystickID_v>(&m_lastInputMethod)) {
		if (m_joysticks.count((SDL_JoystickID) *joystick)) {
			return SDL_RumbleGamepad(
				m_joysticks[(SDL_JoystickID) *joystick].first,
				SDL_clamp(p_lowFrequencyRumble, 0, 1) * USHRT_MAX,
				SDL_clamp(p_highFrequencyRumble, 0, 1) * USHRT_MAX,
				p_milliseconds
			);
		}
	}

	return FALSE;
}

void LegoInputManager::InitializeHaptics()
{
	// We don't get added/removed events for haptic devices that are not joysticks or mice,
	// so we initialize "otherHaptics" once at this point.
	std::vector<SDL_HapticID> existingHaptics;

	for (const auto& [id, mouse] : m_mice) {
		if (mouse.second) {
			existingHaptics.push_back(SDL_GetHapticID(mouse.second));
		}
	}

	for (const auto& [id, joystick] : m_joysticks) {
		if (joystick.second) {
			existingHaptics.push_back(SDL_GetHapticID(joystick.second));
		}
	}

	int count;
	SDL_HapticID* haptics = SDL_GetHaptics(&count);
	if (haptics) {
		for (int i = 0; i < count; i++) {
			if (std::find(existingHaptics.begin(), existingHaptics.end(), haptics[i]) == existingHaptics.end()) {
				SDL_Haptic* haptic = SDL_OpenHaptic(haptics[i]);
				if (haptic) {
					if (SDL_InitHapticRumble(haptic)) {
						m_otherHaptics[haptics[i]] = haptic;
					}
					else {
						SDL_CloseHaptic(haptic);
					}
				}
			}
		}

		SDL_free(haptics);
	}
}

void LegoInputManager::UpdateLastInputMethod(SDL_Event* p_event)
{
	switch (p_event->type) {
	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_UP:
#if SDL_MAJOR_VERSION >= 3
		m_lastInputMethod = SDL_KeyboardID_v{p_event->key.which};
#else
		m_lastInputMethod = SDL_KeyboardID_v{0};
#endif
		break;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
#if SDL_MAJOR_VERSION >= 3
		m_lastInputMethod = SDL_MouseID_v{p_event->button.which};
#else
		m_lastInputMethod = SDL_MouseID_v{0};
#endif
		break;
	case SDL_EVENT_MOUSE_MOTION:
#if SDL_MAJOR_VERSION >= 3
		m_lastInputMethod = SDL_MouseID_v{p_event->motion.which};
#else
		m_lastInputMethod = SDL_MouseID_v{0};
#endif
		break;
	case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
	case SDL_EVENT_GAMEPAD_BUTTON_UP:
		m_lastInputMethod = SDL_JoystickID_v{p_event->gbutton.which};
		break;
	case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		m_lastInputMethod = SDL_JoystickID_v{p_event->gaxis.which};
		break;
	case SDL_EVENT_FINGER_MOTION:
	case SDL_EVENT_FINGER_DOWN:
	case SDL_EVENT_FINGER_UP:
	case SDL_EVENT_FINGER_CANCELED:
		m_lastInputMethod = SDL_TouchID_v{p_event->tfinger.touchID};
		break;
	}
}
