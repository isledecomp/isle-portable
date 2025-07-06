#define INITGUID

#include "isleapp.h"

#include "3dmanager/lego3dmanager.h"
#include "decomp.h"
#include "isledebug.h"
#include "legoanimationmanager.h"
#include "legobuildingmanager.h"
#include "legogamestate.h"
#include "legoinputmanager.h"
#include "legomain.h"
#include "legomodelpresenter.h"
#include "legopartpresenter.h"
#include "legoutils.h"
#include "legovideomanager.h"
#include "legoworldpresenter.h"
#include "misc.h"
#include "mxbackgroundaudiomanager.h"
#include "mxdirectx/mxdirect3d.h"
#include "mxdsaction.h"
#include "mxmisc.h"
#include "mxomnicreateflags.h"
#include "mxomnicreateparam.h"
#include "mxstreamer.h"
#include "mxticklemanager.h"
#include "mxtimer.h"
#include "mxtransitionmanager.h"
#include "mxutilities.h"
#include "mxvariabletable.h"
#include "res/arrow_bmp.h"
#include "res/busy_bmp.h"
#include "res/isle_bmp.h"
#include "res/no_bmp.h"
#include "res/resource.h"
#include "roi/legoroi.h"
#include "tgl/d3drm/impl.h"
#include "viewmanager/viewmanager.h"

#include <miniwin/miniwindevice.h>

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <errno.h>
#include <iniparser.h>
#include <stdlib.h>
#include <time.h>

#ifdef __EMSCRIPTEN__
#include "emscripten/events.h"
#include "emscripten/filesystem.h"
#include "emscripten/messagebox.h"
#endif

#ifdef __3DS__
#include "3ds/apthooks.h"
#include "3ds/config.h"
#endif

DECOMP_SIZE_ASSERT(IsleApp, 0x8c)

// GLOBAL: ISLE 0x410030
IsleApp* g_isle = NULL;

// GLOBAL: ISLE 0x410034
MxU8 g_mousedown = FALSE;

// GLOBAL: ISLE 0x410038
MxU8 g_mousemoved = FALSE;

// GLOBAL: ISLE 0x41003c
MxS32 g_closed = FALSE;

// GLOBAL: ISLE 0x410050
MxS32 g_rmDisabled = FALSE;

// GLOBAL: ISLE 0x410054
MxS32 g_waitingForTargetDepth = TRUE;

// GLOBAL: ISLE 0x410058
MxS32 g_targetWidth = 640;

// GLOBAL: ISLE 0x41005c
MxS32 g_targetHeight = 480;

// GLOBAL: ISLE 0x410060
MxS32 g_targetDepth = 16;

// GLOBAL: ISLE 0x410064
MxS32 g_reqEnableRMDevice = FALSE;

MxFloat g_lastJoystickMouseX = 0;
MxFloat g_lastJoystickMouseY = 0;
MxFloat g_lastMouseX = 320;
MxFloat g_lastMouseY = 240;

bool g_dpadUp = false;
bool g_dpadDown = false;
bool g_dpadLeft = false;
bool g_dpadRight = false;

// STRING: ISLE 0x4101dc
#define WINDOW_TITLE "LEGO®"

SDL_Window* window;

extern const char* g_files[46];

// FUNCTION: ISLE 0x401000
IsleApp::IsleApp()
{
	m_hdPath = NULL;
	m_cdPath = NULL;
	m_deviceId = NULL;
	m_savePath = NULL;
	m_fullScreen = TRUE;
	m_flipSurfaces = FALSE;
	m_backBuffersInVram = TRUE;
	m_using8bit = FALSE;
	m_using16bit = TRUE;
	m_hasLightSupport = FALSE;
	m_drawCursor = FALSE;
	m_use3dSound = TRUE;
	m_useMusic = TRUE;
	m_useJoystick = TRUE;
	m_joystickIndex = 0;
	m_wideViewAngle = TRUE;
	m_islandQuality = 2;
	m_islandTexture = 1;
	m_gameStarted = FALSE;
	m_frameDelta = 10;
	m_windowActive = TRUE;

#ifdef COMPAT_MODE
	{
		MxRect32 r(0, 0, 639, 479);
		MxVideoParamFlags flags;
		m_videoParam = MxVideoParam(r, NULL, 1, flags);
	}
#else
	m_videoParam = MxVideoParam(MxRect32(0, 0, 639, 479), NULL, 1, MxVideoParamFlags());
#endif
	m_videoParam.Flags().Set16Bit(MxDirectDraw::GetPrimaryBitDepth() == 16);

	m_windowHandle = NULL;
	m_cursorArrow = NULL;
	m_cursorBusy = NULL;
	m_cursorNo = NULL;
	m_cursorCurrent = NULL;
	m_cursorArrowBitmap = NULL;
	m_cursorBusyBitmap = NULL;
	m_cursorNoBitmap = NULL;
	m_cursorCurrentBitmap = NULL;

	LegoOmni::CreateInstance();

	m_iniPath = NULL;
	m_maxLod = RealtimeView::GetUserMaxLOD();
	m_maxAllowedExtras = m_islandQuality <= 1 ? 10 : 20;
	m_transitionType = MxTransitionManager::e_mosaic;
	m_cursorSensitivity = 4;
}

// FUNCTION: ISLE 0x4011a0
IsleApp::~IsleApp()
{
	if (LegoOmni::GetInstance()) {
		Close();
		MxOmni::DestroyInstance();
	}

	if (m_hdPath) {
		delete[] m_hdPath;
	}

	if (m_cdPath) {
		delete[] m_cdPath;
	}

	if (m_deviceId) {
		delete[] m_deviceId;
	}

	if (m_savePath) {
		delete[] m_savePath;
	}

	if (m_mediaPath) {
		delete[] m_mediaPath;
	}
}

// FUNCTION: ISLE 0x401260
void IsleApp::Close()
{
	MxDSAction ds;
	ds.SetUnknown24(-2);

	if (Lego()) {
		GameState()->Save(0);
		if (InputManager()) {
			InputManager()->QueueEvent(c_notificationKeyPress, 0, 0, 0, SDLK_SPACE);
		}

		VideoManager()->Get3DManager()->GetLego3DView()->GetViewManager()->RemoveAll(NULL);

		Lego()->RemoveWorld(ds.GetAtomId(), ds.GetObjectId());
		Lego()->DeleteObject(ds);
		TransitionManager()->SetWaitIndicator(NULL);
		Lego()->Resume();

		while (Streamer()->Close(NULL) == SUCCESS) {
		}

		while (Lego() && !Lego()->DoesEntityExist(ds)) {
			Timer()->GetRealTime();
			TickleManager()->Tickle();
		}
	}
}

// FUNCTION: ISLE 0x4013b0
MxS32 IsleApp::SetupLegoOmni()
{
	MxS32 result = FALSE;

#ifdef COMPAT_MODE
	MxS32 failure;
	{
		MxOmniCreateParam param(m_mediaPath, m_windowHandle, m_videoParam, MxOmniCreateFlags());
		failure = Lego()->Create(param) == FAILURE;
	}
#else
	MxS32 failure =
		Lego()->Create(MxOmniCreateParam(m_mediaPath, m_windowHandle, m_videoParam, MxOmniCreateFlags())) == FAILURE;
#endif

	if (!failure) {
		VariableTable()->SetVariable("ACTOR_01", "");
		TickleManager()->SetClientTickleInterval(VideoManager(), 10);
		result = TRUE;
	}

	return result;
}

// FUNCTION: ISLE 0x401560
void IsleApp::SetupVideoFlags(
	MxS32 fullScreen,
	MxS32 flipSurfaces,
	MxS32 backBuffers,
	MxS32 using8bit,
	MxS32 using16bit,
	MxS32 hasLightSupport,
	MxS32 param_7,
	MxS32 wideViewAngle,
	char* deviceId
)
{
	m_videoParam.Flags().SetFullScreen(fullScreen);
	m_videoParam.Flags().SetFlipSurfaces(flipSurfaces);
	m_videoParam.Flags().SetBackBuffers(!backBuffers);
	m_videoParam.Flags().SetLacksLightSupport(!hasLightSupport);
	m_videoParam.Flags().SetF1bit7(param_7);
	m_videoParam.Flags().SetWideViewAngle(wideViewAngle);
	m_videoParam.Flags().SetF2bit1(1);
	m_videoParam.SetDeviceName(deviceId);
	if (using8bit) {
		m_videoParam.Flags().Set16Bit(0);
	}
	if (using16bit) {
		m_videoParam.Flags().Set16Bit(1);
	}
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv)
{
	*appstate = NULL;

	SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
		char buffer[256];
		SDL_snprintf(
			buffer,
			sizeof(buffer),
			"\"LEGO® Island\" failed to start.\nPlease quit all other applications and try again.\nSDL error: %s",
			SDL_GetError()
		);
		Any_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "LEGO® Island Error", buffer, NULL);
		return SDL_APP_FAILURE;
	}

	// [library:window]
	// Original game checks for an existing instance here.
	// We don't really need that.

	// Create global app instance
	g_isle = new IsleApp();

	if (g_isle->ParseArguments(argc, argv) != SUCCESS) {
		Any_ShowSimpleMessageBox(
			SDL_MESSAGEBOX_ERROR,
			"LEGO® Island Error",
			"\"LEGO® Island\" failed to start.  Invalid CLI arguments.",
			window
		);
		return SDL_APP_FAILURE;
	}

	// Create window
	if (g_isle->SetupWindow() != SUCCESS) {
		Any_ShowSimpleMessageBox(
			SDL_MESSAGEBOX_ERROR,
			"LEGO® Island Error",
			"\"LEGO® Island\" failed to start.\nPlease quit all other applications and try again.",
			window
		);
		return SDL_APP_FAILURE;
	}

	// Get reference to window
	*appstate = g_isle->GetWindowHandle();

#ifdef __EMSCRIPTEN__
	SDL_AddEventWatch(
		[](void* userdata, SDL_Event* event) -> bool {
			if (event->type == SDL_EVENT_TERMINATING && g_isle && g_isle->GetGameStarted()) {
				GameState()->Save(0);
				return false;
			}

			return true;
		},
		NULL
	);
#endif
#ifdef __3DS__
	N3DS_SetupAptHooks();
#endif
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
	if (g_closed) {
		return SDL_APP_SUCCESS;
	}

	if (!g_isle->Tick()) {
		Any_ShowSimpleMessageBox(
			SDL_MESSAGEBOX_ERROR,
			"LEGO® Island Error",
			"\"LEGO® Island\" failed to start.\nPlease quit all other applications and try again."
			"\nFailed to initialize; see logs for details",
			NULL
		);
		return SDL_APP_FAILURE;
	}

	if (!g_closed) {
		IsleDebug_Render();

		if (g_reqEnableRMDevice) {
			g_reqEnableRMDevice = FALSE;
			VideoManager()->EnableRMDevice();
			g_rmDisabled = FALSE;
			Lego()->Resume();
		}

		if (g_closed) {
			return SDL_APP_SUCCESS;
		}

		if (g_mousedown && g_mousemoved && g_isle) {
			if (!g_isle->Tick()) {
				Any_ShowSimpleMessageBox(
					SDL_MESSAGEBOX_ERROR,
					"LEGO® Island Error",
					"\"LEGO® Island\" failed to start.\nPlease quit all other applications and try again."
					"\nFailed to initialize; see logs for details",
					NULL
				);
				return SDL_APP_FAILURE;
			}
		}

		if (g_mousemoved) {
			g_mousemoved = FALSE;
		}

		g_isle->MoveVirtualMouseViaJoystick();
	}

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
	if (!g_isle) {
		return SDL_APP_CONTINUE;
	}

	if (IsleDebug_Event(event)) {
		return SDL_APP_CONTINUE;
	}

	// [library:window]
	// Remaining functionality to be implemented:
	// WM_TIMER - use SDL_Timer functionality instead

#ifdef __EMSCRIPTEN__
	// Workaround for the fact we are getting both mouse & touch events on mobile devices running Emscripten.
	// On desktops, we are only getting mouse events, but a touch device (pen_input) may also be present...
	// See: https://github.com/libsdl-org/SDL/issues/13161
	static bool detectedTouchEvents = false;
#endif

	switch (event->type) {
	case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
	case SDL_EVENT_MOUSE_MOTION:
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
		IDirect3DRMMiniwinDevice* device = GetD3DRMMiniwinDevice();
		if (device && !device->ConvertEventToRenderCoordinates(event)) {
			SDL_Log("Failed to convert event coordinates: %s", SDL_GetError());
		}
		break;
	}

	switch (event->type) {
	case SDL_EVENT_WINDOW_FOCUS_GAINED:
		if (!IsleDebug_Enabled()) {
			g_isle->SetWindowActive(TRUE);
			Lego()->Resume();
		}
		break;
	case SDL_EVENT_WINDOW_FOCUS_LOST:
		if (!IsleDebug_Enabled() && g_isle->GetGameStarted()) {
			g_isle->SetWindowActive(FALSE);
			Lego()->Pause();
#ifdef __EMSCRIPTEN__
			GameState()->Save(0);
#endif
		}
		break;
	case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
		if (!g_closed) {
			delete g_isle;
			g_isle = NULL;
			g_closed = TRUE;
		}
		break;
	case SDL_EVENT_KEY_DOWN: {
		if (event->key.repeat) {
			break;
		}

		SDL_Keycode keyCode = event->key.key;

		if ((event->key.mod & SDL_KMOD_LALT) && keyCode == SDLK_RETURN) {
			SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
		}
		else {
			if (InputManager()) {
				InputManager()->QueueEvent(c_notificationKeyPress, keyCode, 0, 0, keyCode);
			}
		}
		break;
	}
	case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
		switch (event->gbutton.button) {
		case SDL_GAMEPAD_BUTTON_DPAD_UP:
			g_dpadUp = true;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
			g_dpadDown = true;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
			g_dpadLeft = true;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
			g_dpadRight = true;
			break;
		case SDL_GAMEPAD_BUTTON_EAST:
			g_mousedown = TRUE;
			if (InputManager()) {
				InputManager()->QueueEvent(
					c_notificationButtonDown,
					LegoEventNotificationParam::c_lButtonState,
					g_lastMouseX,
					g_lastMouseY,
					0
				);
			}
			break;

		case SDL_GAMEPAD_BUTTON_SOUTH:
			if (InputManager()) {
				InputManager()->QueueEvent(c_notificationKeyPress, SDLK_SPACE, 0, 0, SDLK_SPACE);
			}
			break;

		case SDL_GAMEPAD_BUTTON_START:
			if (InputManager()) {
				InputManager()->QueueEvent(c_notificationKeyPress, SDLK_ESCAPE, 0, 0, SDLK_ESCAPE);
			}
			break;
		}
		break;
	}

	case SDL_EVENT_GAMEPAD_BUTTON_UP: {
		switch (event->gbutton.button) {
		case SDL_GAMEPAD_BUTTON_DPAD_UP:
			g_dpadUp = false;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
			g_dpadDown = false;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
			g_dpadLeft = false;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
			g_dpadRight = false;
			break;
		case SDL_GAMEPAD_BUTTON_EAST:
			g_mousedown = FALSE;
			if (InputManager()) {
				InputManager()->QueueEvent(
					c_notificationButtonUp,
					LegoEventNotificationParam::c_lButtonState,
					g_lastMouseX,
					g_lastMouseY,
					0
				);
			}
			break;
		}
		break;
	}
	case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
		MxS16 axisValue = 0;
		if (event->gaxis.value < -8000 || event->gaxis.value > 8000) {
			// Ignore small axis values
			axisValue = event->gaxis.value;
		}
		if (event->gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTX) {
			g_lastJoystickMouseX = ((MxFloat) axisValue) / SDL_JOYSTICK_AXIS_MAX * g_isle->GetCursorSensitivity();
		}
		else if (event->gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTY) {
			g_lastJoystickMouseY = ((MxFloat) axisValue) / SDL_JOYSTICK_AXIS_MAX * g_isle->GetCursorSensitivity();
		}
		else if (event->gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
			if (axisValue != 0 && !g_mousedown) {
				g_mousedown = TRUE;

				if (InputManager()) {
					InputManager()->QueueEvent(
						c_notificationButtonDown,
						LegoEventNotificationParam::c_lButtonState,
						g_lastMouseX,
						g_lastMouseY,
						0
					);
				}
			}
			else if (axisValue == 0 && g_mousedown) {
				g_mousedown = FALSE;

				if (InputManager()) {
					InputManager()->QueueEvent(
						c_notificationButtonUp,
						LegoEventNotificationParam::c_lButtonState,
						g_lastMouseX,
						g_lastMouseY,
						0
					);
				}
			}
		}
		break;
	}
	case SDL_EVENT_MOUSE_MOTION:
#ifdef __EMSCRIPTEN__
		if (detectedTouchEvents) {
			break;
		}
#endif
		g_mousemoved = TRUE;

		if (InputManager()) {
			InputManager()->QueueEvent(
				c_notificationMouseMove,
				IsleApp::MapMouseButtonFlagsToModifier(event->motion.state),
				event->motion.x,
				event->motion.y,
				0
			);
		}

		if (g_isle->GetDrawCursor()) {
			VideoManager()->MoveCursor(Min((MxS32) event->motion.x, 639), Min((MxS32) event->motion.y, 479));
		}
		break;
	case SDL_EVENT_FINGER_MOTION: {
#ifdef __EMSCRIPTEN__
		detectedTouchEvents = true;
#endif
		g_mousemoved = TRUE;

		float x = SDL_clamp(event->tfinger.x, 0, 1) * 640;
		float y = SDL_clamp(event->tfinger.y, 0, 1) * 480;

		if (InputManager()) {
			InputManager()->QueueEvent(c_notificationMouseMove, LegoEventNotificationParam::c_lButtonState, x, y, 0);
		}

		if (g_isle->GetDrawCursor()) {
			VideoManager()->MoveCursor(Min((MxS32) x, 639), Min((MxS32) y, 479));
		}
		break;
	}
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
#ifdef __EMSCRIPTEN__
		if (detectedTouchEvents) {
			break;
		}
#endif
		g_mousedown = TRUE;

		if (InputManager()) {
			InputManager()->QueueEvent(
				c_notificationButtonDown,
				IsleApp::MapMouseButtonFlagsToModifier(SDL_GetMouseState(NULL, NULL)),
				event->button.x,
				event->button.y,
				0
			);
		}
		break;
	case SDL_EVENT_FINGER_DOWN: {
#ifdef __EMSCRIPTEN__
		detectedTouchEvents = true;
#endif
		g_mousedown = TRUE;

		float x = SDL_clamp(event->tfinger.x, 0, 1) * 640;
		float y = SDL_clamp(event->tfinger.y, 0, 1) * 480;

		if (InputManager()) {
			InputManager()->QueueEvent(c_notificationButtonDown, LegoEventNotificationParam::c_lButtonState, x, y, 0);
		}
		break;
	}
	case SDL_EVENT_MOUSE_BUTTON_UP:
#ifdef __EMSCRIPTEN__
		if (detectedTouchEvents) {
			// Abusing the fact (bug?) that we are always getting mouse events on Emscripten.
			// This functionality should be enabled in a more general way with touch events,
			// but SDL touch event's don't have a "double tap" indicator right now.
			if (event->button.clicks == 2) {
				InputManager()->QueueEvent(c_notificationKeyPress, SDLK_SPACE, 0, 0, SDLK_SPACE);
			}
			break;
		}
#endif
		g_mousedown = FALSE;

		if (InputManager()) {
			InputManager()->QueueEvent(
				c_notificationButtonUp,
				IsleApp::MapMouseButtonFlagsToModifier(SDL_GetMouseState(NULL, NULL)),
				event->button.x,
				event->button.y,
				0
			);
		}
		break;
	case SDL_EVENT_FINGER_UP: {
#ifdef __EMSCRIPTEN__
		detectedTouchEvents = true;
#endif
		g_mousedown = FALSE;

		float x = SDL_clamp(event->tfinger.x, 0, 1) * 640;
		float y = SDL_clamp(event->tfinger.y, 0, 1) * 480;

		if (InputManager()) {
			InputManager()->QueueEvent(c_notificationButtonUp, 0, x, y, 0);
		}
		break;
	}
	case SDL_EVENT_QUIT:
		return SDL_APP_SUCCESS;
		break;
	}

	if (event->user.type == g_legoSdlEvents.m_windowsMessage) {
		switch (event->user.code) {
		case WM_ISLE_SETCURSOR:
			g_isle->SetupCursor((Cursor) (uintptr_t) event->user.data1);
			break;
		case WM_TIMER:
			if (InputManager()) {
				InputManager()->QueueEvent(c_notificationTimer, (MxU8) (uintptr_t) event->user.data1, 0, 0, 0);
			}
			break;
		default:
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unknown SDL Windows message: 0x%" SDL_PRIx32, event->user.code);
			break;
		}
	}
	else if (event->user.type == g_legoSdlEvents.m_presenterProgress) {
		MxDSAction* action = static_cast<MxDSAction*>(event->user.data1);
		MxPresenter::TickleState state = static_cast<MxPresenter::TickleState>(event->user.code);

#ifdef __EMSCRIPTEN__
		if (!g_isle->GetGameStarted()) {
			Emscripten_SendPresenterProgress(action, state);
		}
#endif

		if (!g_isle->GetGameStarted() && action && state == MxPresenter::e_ready &&
			!SDL_strncmp(action->GetObjectName(), "Lego_Smk", 8)) {
			g_isle->SetGameStarted(TRUE);
			SDL_Log("Game started");
		}
	}

	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
	IsleDebug_Quit();

	if (appstate != NULL) {
		SDL_DestroyWindow((SDL_Window*) appstate);
	}

	SDL_Quit();
}

MxU8 IsleApp::MapMouseButtonFlagsToModifier(SDL_MouseButtonFlags p_flags)
{
	// [library:window]
	// Map button states to Windows button states (LegoEventNotificationParam)
	// Not mapping mod keys SHIFT and CTRL since they are not used by the game.

	MxU8 modifier = 0;
	if (p_flags & SDL_BUTTON_LMASK) {
		modifier |= LegoEventNotificationParam::c_lButtonState;
	}
	if (p_flags & SDL_BUTTON_RMASK) {
		modifier |= LegoEventNotificationParam::c_rButtonState;
	}

	return modifier;
}

// FUNCTION: ISLE 0x4023e0
MxResult IsleApp::SetupWindow()
{
	if (!LoadConfig()) {
		return FAILURE;
	}

	SetupVideoFlags(
		m_fullScreen,
		m_flipSurfaces,
		m_backBuffersInVram,
		m_using8bit,
		m_using16bit,
		m_hasLightSupport,
		FALSE,
		m_wideViewAngle,
		m_deviceId
	);

	MxOmni::SetSound3D(m_use3dSound);

	srand(time(NULL));

	// [library:window] Use original game cursors in the resources instead?
	m_cursorCurrent = m_cursorArrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
	m_cursorBusy = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT);
	m_cursorNo = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NOT_ALLOWED);
	SDL_SetCursor(m_cursorCurrent);
	if (g_isle->GetDrawCursor()) {
		SDL_HideCursor();
		m_cursorCurrentBitmap = m_cursorArrowBitmap = &arrow_cursor;
		m_cursorBusyBitmap = &busy_cursor;
		m_cursorNoBitmap = &no_cursor;
	}

	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, g_targetWidth);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, g_targetHeight);
	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, m_fullScreen);
	SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, WINDOW_TITLE);
#if defined(MINIWIN) && !defined(__3DS__)
	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
#endif

	window = SDL_CreateWindowWithProperties(props);
#ifdef MINIWIN
	m_windowHandle = reinterpret_cast<HWND>(window);
#else
	m_windowHandle =
		(HWND) SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
#endif

	SDL_DestroyProperties(props);

	if (!m_windowHandle) {
		return FAILURE;
	}

	SDL_IOStream* icon_stream = SDL_IOFromMem(isle_bmp, isle_bmp_len);

	if (icon_stream) {
		SDL_Surface* icon = SDL_LoadBMP_IO(icon_stream, true);

		if (icon) {
			SDL_SetWindowIcon(window, icon);
			SDL_DestroySurface(icon);
		}
		else {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load icon: %s", SDL_GetError());
		}
	}
	else {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open SDL_IOStream for icon: %s", SDL_GetError());
	}

	if (!SetupLegoOmni()) {
		return FAILURE;
	}

	GameState()->SetSavePath(m_savePath);

	if (VerifyFilesystem() != SUCCESS) {
		return FAILURE;
	}

	DetectGameVersion();
	GameState()->SerializePlayersInfo(LegoStorage::c_read);
	GameState()->SerializeScoreHistory(LegoStorage::c_read);

	MxS32 iVar10;
	switch (m_islandQuality) {
	case 0:
		iVar10 = 1;
		break;
	case 1:
		iVar10 = 2;
		break;
	default:
		iVar10 = 100;
	}

	MxS32 uVar1 = (m_islandTexture == 0);
	LegoModelPresenter::configureLegoModelPresenter(uVar1);
	LegoPartPresenter::configureLegoPartPresenter(uVar1, iVar10);
	LegoWorldPresenter::configureLegoWorldPresenter(m_islandQuality);
	LegoBuildingManager::configureLegoBuildingManager(m_islandQuality);
	LegoROI::configureLegoROI(iVar10);
	LegoAnimationManager::configureLegoAnimationManager(m_maxAllowedExtras);
	MxTransitionManager::configureMxTransitionManager(m_transitionType);
	RealtimeView::SetUserMaxLOD(m_maxLod);
	if (LegoOmni::GetInstance()) {
		if (LegoOmni::GetInstance()->GetInputManager()) {
			LegoOmni::GetInstance()->GetInputManager()->SetUseJoystick(m_useJoystick);
			LegoOmni::GetInstance()->GetInputManager()->SetJoystickIndex(m_joystickIndex);
		}
		if (LegoOmni::GetInstance()->GetVideoManager() && g_isle->GetDrawCursor()) {
			LegoOmni::GetInstance()->GetVideoManager()->SetCursorBitmap(m_cursorCurrentBitmap);
		}
		MxDirect3D* d3d = LegoOmni::GetInstance()->GetVideoManager()->GetDirect3D();
		if (d3d) {
			SDL_Log(
				"Direct3D driver name=\"%s\" description=\"%s\"",
				d3d->GetDeviceName().c_str(),
				d3d->GetDeviceDescription().c_str()
			);
		}
		else {
			SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to get D3D device name and description");
		}
	}

	IsleDebug_Init();

	return SUCCESS;
}

// FUNCTION: ISLE 0x4028d0
bool IsleApp::LoadConfig()
{
	char* prefPath = SDL_GetPrefPath("isledecomp", "isle");
	char* iniConfig;

#ifdef __EMSCRIPTEN__
	if (m_iniPath && !Emscripten_SetupConfig(m_iniPath)) {
		m_iniPath = NULL;
	}
#endif

	if (m_iniPath) {
		iniConfig = new char[strlen(m_iniPath) + 1];
		strcpy(iniConfig, m_iniPath);
	}
	else if (prefPath) {
		iniConfig = new char[strlen(prefPath) + strlen("isle.ini") + 1]();
		strcat(iniConfig, prefPath);
		strcat(iniConfig, "isle.ini");
	}
	else {
		iniConfig = new char[strlen("isle.ini") + 1];
		strcpy(iniConfig, "isle.ini");
	}
	SDL_Log("Reading configuration from \"%s\"", iniConfig);

	dictionary* dict = iniparser_load(iniConfig);

	// [library:config]
	// Load sane defaults if dictionary failed to load
	if (!dict) {
		if (m_iniPath) {
			SDL_Log("Invalid config path '%s'", m_iniPath);
			return false;
		}

		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Loading sane defaults");
		FILE* iniFP = fopen(iniConfig, "wb");

		if (!iniFP) {
			SDL_LogError(
				SDL_LOG_CATEGORY_APPLICATION,
				"Failed to write config at '%s': %s",
				iniConfig,
				strerror(errno)
			);
			return false;
		}

		char buf[32];
		dict = iniparser_load(iniConfig);
		iniparser_set(dict, "isle", NULL);

		iniparser_set(dict, "isle:diskpath", SDL_GetBasePath());
		iniparser_set(dict, "isle:cdpath", MxOmni::GetCD());
		iniparser_set(dict, "isle:mediapath", SDL_GetBasePath());
		iniparser_set(dict, "isle:savepath", prefPath);

		iniparser_set(dict, "isle:Flip Surfaces", m_flipSurfaces ? "true" : "false");
		iniparser_set(dict, "isle:Full Screen", m_fullScreen ? "true" : "false");
		iniparser_set(dict, "isle:Wide View Angle", m_wideViewAngle ? "true" : "false");

		iniparser_set(dict, "isle:3DSound", m_use3dSound ? "true" : "false");
		iniparser_set(dict, "isle:Music", m_useMusic ? "true" : "false");

		iniparser_set(dict, "isle:UseJoystick", m_useJoystick ? "true" : "false");
		iniparser_set(dict, "isle:JoystickIndex", SDL_itoa(m_joystickIndex, buf, 10));
		iniparser_set(dict, "isle:Draw Cursor", m_drawCursor ? "true" : "false");
		SDL_snprintf(buf, sizeof(buf), "%f", m_cursorSensitivity);
		iniparser_set(dict, "isle:Cursor Sensitivity", buf);

		iniparser_set(dict, "isle:Back Buffers in Video RAM", "-1");

		iniparser_set(dict, "isle:Island Quality", SDL_itoa(m_islandQuality, buf, 10));
		iniparser_set(dict, "isle:Island Texture", SDL_itoa(m_islandTexture, buf, 10));
		SDL_snprintf(buf, sizeof(buf), "%f", m_maxLod);
		iniparser_set(dict, "isle:Max LOD", buf);
		iniparser_set(dict, "isle:Max Allowed Extras", SDL_itoa(m_maxAllowedExtras, buf, 10));
		iniparser_set(dict, "isle:Transition Type", SDL_itoa(m_transitionType, buf, 10));

#ifdef __3DS__
		N3DS_SetupDefaultConfigOverrides(dict);
#endif
		iniparser_dump_ini(dict, iniFP);
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "New config written at '%s'", iniConfig);
		fclose(iniFP);
	}

#ifdef __EMSCRIPTEN__
	const char* hdPath = Emscripten_bundledPath;
#else
	const char* hdPath = iniparser_getstring(dict, "isle:diskpath", SDL_GetBasePath());
#endif
	m_hdPath = new char[strlen(hdPath) + 1];
	strcpy(m_hdPath, hdPath);
	MxOmni::SetHD(m_hdPath);

#ifdef __EMSCRIPTEN__
	const char* cdPath = Emscripten_streamPath;
#else
	const char* cdPath = iniparser_getstring(dict, "isle:cdpath", MxOmni::GetCD());
#endif
	m_cdPath = new char[strlen(cdPath) + 1];
	strcpy(m_cdPath, cdPath);
	MxOmni::SetCD(m_cdPath);

	const char* mediaPath = iniparser_getstring(dict, "isle:mediapath", hdPath);
	m_mediaPath = new char[strlen(mediaPath) + 1];
	strcpy(m_mediaPath, mediaPath);

	m_flipSurfaces = iniparser_getboolean(dict, "isle:Flip Surfaces", m_flipSurfaces);

#ifdef __EMSCRIPTEN__
	m_fullScreen = FALSE;
#else
	m_fullScreen = iniparser_getboolean(dict, "isle:Full Screen", m_fullScreen);
#endif

	m_wideViewAngle = iniparser_getboolean(dict, "isle:Wide View Angle", m_wideViewAngle);
	m_use3dSound = iniparser_getboolean(dict, "isle:3DSound", m_use3dSound);
	m_useMusic = iniparser_getboolean(dict, "isle:Music", m_useMusic);
	m_useJoystick = iniparser_getboolean(dict, "isle:UseJoystick", m_useJoystick);
	m_joystickIndex = iniparser_getint(dict, "isle:JoystickIndex", m_joystickIndex);
	m_drawCursor = iniparser_getboolean(dict, "isle:Draw Cursor", m_drawCursor);
	m_cursorSensitivity = iniparser_getdouble(dict, "isle:Cursor Sensitivity", m_cursorSensitivity);

	MxS32 backBuffersInVRAM = iniparser_getboolean(dict, "isle:Back Buffers in Video RAM", -1);
	if (backBuffersInVRAM != -1) {
		m_backBuffersInVram = !backBuffersInVRAM;
	}

	MxS32 bitDepth = iniparser_getint(dict, "isle:Display Bit Depth", -1);
	if (bitDepth != -1) {
		if (bitDepth == 8) {
			m_using8bit = TRUE;
		}
		else if (bitDepth == 16) {
			m_using16bit = TRUE;
		}
	}

	m_islandQuality = iniparser_getint(dict, "isle:Island Quality", m_islandQuality);
	m_islandTexture = iniparser_getint(dict, "isle:Island Texture", m_islandTexture);
	m_maxLod = iniparser_getdouble(dict, "isle:Max LOD", m_maxLod);
	m_maxAllowedExtras = iniparser_getint(dict, "isle:Max Allowed Extras", m_maxAllowedExtras);
	m_transitionType =
		(MxTransitionManager::TransitionType) iniparser_getint(dict, "isle:Transition Type", m_transitionType);

	const char* deviceId = iniparser_getstring(dict, "isle:3D Device ID", NULL);
	if (deviceId != NULL) {
		m_deviceId = new char[strlen(deviceId) + 1];
		strcpy(m_deviceId, deviceId);
	}

	// [library:config]
	// The original game does not save any data if no savepath is given.
	// Instead, we use SDLs prefPath as a default fallback and always save data.
#ifdef __EMSCRIPTEN__
	const char* savePath = Emscripten_savePath;
#else
	const char* savePath = iniparser_getstring(dict, "isle:savepath", prefPath);
#endif
	m_savePath = new char[strlen(savePath) + 1];
	strcpy(m_savePath, savePath);

	iniparser_freedict(dict);
	delete[] iniConfig;
	SDL_free(prefPath);

	return true;
}

// FUNCTION: ISLE 0x402c20
inline bool IsleApp::Tick()
{
	// GLOBAL: ISLE 0x4101c0
	static MxLong g_lastFrameTime = 0;

	// GLOBAL: ISLE 0x4101bc
	static MxS32 g_startupDelay = 1;

	if (IsleDebug_Paused() && IsleDebug_StepModeEnabled()) {
		IsleDebug_SetPaused(false);
	}

	if (!m_windowActive) {
		SDL_Delay(1);
		return true;
	}

	if (!Lego()) {
		return true;
	}
	if (!TickleManager()) {
		return true;
	}
	if (!Timer()) {
		return true;
	}

	MxLong currentTime = Timer()->GetRealTime();
	if (currentTime < g_lastFrameTime) {
		g_lastFrameTime = -m_frameDelta;
	}

	if (m_frameDelta + g_lastFrameTime >= currentTime) {
		SDL_Delay(1);
		return true;
	}

	if (!Lego()->IsPaused()) {
		TickleManager()->Tickle();
	}
	g_lastFrameTime = currentTime;

	if (IsleDebug_StepModeEnabled()) {
		IsleDebug_SetPaused(true);
		IsleDebug_ResetStepMode();
	}

	if (g_startupDelay == 0) {
		return true;
	}

	g_startupDelay--;
	if (g_startupDelay != 0) {
		return true;
	}

	LegoOmni::GetInstance()->CreateBackgroundAudio();
	BackgroundAudioManager()->Enable(m_useMusic);

	MxStreamController* stream = Streamer()->Open("\\lego\\scripts\\isle\\isle", MxStreamer::e_diskStream);
	MxDSAction ds;

	if (!stream) {
		stream = Streamer()->Open("\\lego\\scripts\\nocd", MxStreamer::e_diskStream);
		if (!stream) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open NOCD.si: Streamer failed to load");
			return false;
		}

		ds.SetAtomId(stream->GetAtom());
		ds.SetUnknown24(-1);
		ds.SetObjectId(0);
		VideoManager()->EnableFullScreenMovie(TRUE, TRUE);

		if (Start(&ds) != SUCCESS) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open NOCD.si: Failed to start initial action");
			return false;
		}
	}
	else {
		ds.SetAtomId(stream->GetAtom());
		ds.SetUnknown24(-1);
		ds.SetObjectId(0);
		if (Start(&ds) != SUCCESS) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open ISLE.si: Failed to start initial action");
			return false;
		}
	}

	return true;
}

// FUNCTION: ISLE 0x402e80
void IsleApp::SetupCursor(Cursor p_cursor)
{
	switch (p_cursor) {
	case e_cursorArrow:
		m_cursorCurrent = m_cursorArrow;
		m_cursorCurrentBitmap = m_cursorArrowBitmap;
		break;
	case e_cursorBusy:
		m_cursorCurrent = m_cursorBusy;
		m_cursorCurrentBitmap = m_cursorBusyBitmap;
		break;
	case e_cursorNo:
		m_cursorCurrent = m_cursorNo;
		m_cursorCurrentBitmap = m_cursorNoBitmap;
		break;
	case e_cursorNone:
		m_cursorCurrent = NULL;
		m_cursorCurrentBitmap = NULL;
	case e_cursorUnused3:
	case e_cursorUnused4:
	case e_cursorUnused5:
	case e_cursorUnused6:
	case e_cursorUnused7:
	case e_cursorUnused8:
	case e_cursorUnused9:
	case e_cursorUnused10:
		break;
	}

	if (g_isle->GetDrawCursor()) {
		if (m_cursorCurrentBitmap == NULL) {
			VideoManager()->SetCursorBitmap(NULL);
		}
		else {
			VideoManager()->SetCursorBitmap(m_cursorCurrentBitmap);
		}
	}
	else {
		if (m_cursorCurrent != NULL) {
			SDL_SetCursor(m_cursorCurrent);
			SDL_ShowCursor();
		}
		else {
			SDL_HideCursor();
		}
	}
}

MxResult IsleApp::ParseArguments(int argc, char** argv)
{
	for (int i = 1, consumed; i < argc; i += consumed) {
		consumed = -1;

		if (strcmp(argv[i], "--ini") == 0 && i + 1 < argc) {
			m_iniPath = argv[i + 1];
			consumed = 2;
		}
		else if (strcmp(argv[i], "--debug") == 0) {
#ifdef ISLE_DEBUG
			IsleDebug_SetEnabled(true);
#else
			SDL_Log("isle is built without debug support. Ignoring --debug argument.");
#endif
			consumed = 1;
		}
		if (consumed <= 0) {
			SDL_Log("Invalid argument(s): %s", argv[i]);
			return FAILURE;
		}
	}

	return SUCCESS;
}

MxResult IsleApp::VerifyFilesystem()
{
#ifdef __EMSCRIPTEN__
	Emscripten_SetupFilesystem();
#else
	for (const char* file : g_files) {
		const char* searchPaths[] = {".", m_hdPath, m_cdPath};
		bool found = false;

		for (const char* base : searchPaths) {
			MxString path(base);
			path += file;
			path.MapPathToFilesystem();

			if (SDL_GetPathInfo(path.GetData(), NULL)) {
				found = true;
				break;
			}
		}

		if (!found) {
			char buffer[1024];
			SDL_snprintf(
				buffer,
				sizeof(buffer),
				"\"LEGO® Island\" failed to start.\nPlease make sure the file %s is located in either diskpath or "
				"cdpath.\nSDL error: %s",
				file,
				SDL_GetError()
			);

			Any_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "LEGO® Island Error", buffer, NULL);
			return FAILURE;
		}
	}
#endif

	return SUCCESS;
}

void IsleApp::DetectGameVersion()
{
	const char* file = "/lego/scripts/infocntr/infomain.si";
	SDL_PathInfo info;
	bool success = false;

	MxString path = MxString(m_hdPath) + file;
	path.MapPathToFilesystem();
	if (!(success = SDL_GetPathInfo(path.GetData(), &info))) {
		path = MxString(m_cdPath) + file;
		path.MapPathToFilesystem();
		success = SDL_GetPathInfo(path.GetData(), &info);
	}

	assert(success);

	// File sizes of INFOMAIN.SI in English 1.0 and Japanese 1.0
	Lego()->SetVersion10(info.size == 58130432 || info.size == 57737216);

	if (Lego()->IsVersion10()) {
		SDL_Log("Detected game version 1.0");
		SDL_SetWindowTitle(reinterpret_cast<SDL_Window*>(m_windowHandle), "Lego Island");
	}
	else {
		SDL_Log("Detected game version 1.1");
	}
}

IDirect3DRMMiniwinDevice* GetD3DRMMiniwinDevice()
{
	LegoVideoManager* videoManager = LegoOmni::GetInstance()->GetVideoManager();
	if (!videoManager) {
		return nullptr;
	}
	Lego3DManager* lego3DManager = videoManager->Get3DManager();
	if (!lego3DManager) {
		return nullptr;
	}
	Lego3DView* lego3DView = lego3DManager->GetLego3DView();
	if (!lego3DView) {
		return nullptr;
	}
	TglImpl::DeviceImpl* tgl_device = (TglImpl::DeviceImpl*) lego3DView->GetDevice();
	if (!tgl_device) {
		return nullptr;
	}
	IDirect3DRMDevice2* d3drmdev = tgl_device->ImplementationData();
	if (!d3drmdev) {
		return nullptr;
	}
	IDirect3DRMMiniwinDevice* d3drmMiniwinDev = nullptr;
	if (!SUCCEEDED(d3drmdev->QueryInterface(IID_IDirect3DRMMiniwinDevice, (void**) &d3drmMiniwinDev))) {
		return nullptr;
	}
	return d3drmMiniwinDev;
}

void IsleApp::MoveVirtualMouseViaJoystick()
{
	float dpadX = 0.0f;
	float dpadY = 0.0f;

	if (g_dpadLeft) {
		dpadX -= m_cursorSensitivity;
	}
	if (g_dpadRight) {
		dpadX += m_cursorSensitivity;
	}
	if (g_dpadUp) {
		dpadY -= m_cursorSensitivity;
	}
	if (g_dpadDown) {
		dpadY += m_cursorSensitivity;
	}

	// Use joystick axis if non-zero, else fall back to dpad
	float moveX = (g_lastJoystickMouseX != 0) ? g_lastJoystickMouseX : dpadX;
	float moveY = (g_lastJoystickMouseY != 0) ? g_lastJoystickMouseY : dpadY;

	if (moveX != 0 || moveY != 0) {
		g_mousemoved = TRUE;

		g_lastMouseX = SDL_clamp(g_lastMouseX + moveX, 0, 640);
		g_lastMouseY = SDL_clamp(g_lastMouseY + moveY, 0, 480);

		if (InputManager()) {
			InputManager()->QueueEvent(
				c_notificationMouseMove,
				g_mousedown ? LegoEventNotificationParam::c_lButtonState : 0,
				g_lastMouseX,
				g_lastMouseY,
				0
			);
		}

		if (g_isle->GetDrawCursor()) {
			VideoManager()->MoveCursor(Min((MxS32) g_lastMouseX, 639), Min((MxS32) g_lastMouseY, 479));
		}
	}
}
