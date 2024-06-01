#include "isleapp.h"

#include "3dmanager/lego3dmanager.h"
#include "decomp.h"
#include "legoanimationmanager.h"
#include "legobuildingmanager.h"
#include "legogamestate.h"
#include "legoinputmanager.h"
#include "legomain.h"
#include "legomodelpresenter.h"
#include "legopartpresenter.h"
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
#include "res/resource.h"
#include "roi/legoroi.h"
#include "viewmanager/viewmanager.h"

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_messagebox.h>
#include <dsound.h>
#include <iniparser.h>
#include <time.h>

DECOMP_SIZE_ASSERT(IsleApp, 0x8c)

// GLOBAL: ISLE 0x410030
IsleApp* g_isle = NULL;

// GLOBAL: ISLE 0x410034
unsigned char g_mousedown = FALSE;

// GLOBAL: ISLE 0x410038
unsigned char g_mousemoved = FALSE;

// GLOBAL: ISLE 0x41003c
BOOL g_closed = FALSE;

// GLOBAL: ISLE 0x410040
RECT g_windowRect = {0, 0, 640, 480};

// GLOBAL: ISLE 0x410050
BOOL g_rmDisabled = FALSE;

// GLOBAL: ISLE 0x410054
BOOL g_waitingForTargetDepth = TRUE;

// GLOBAL: ISLE 0x410058
int g_targetWidth = 640;

// GLOBAL: ISLE 0x41005c
int g_targetHeight = 480;

// GLOBAL: ISLE 0x410060
int g_targetDepth = 16;

// GLOBAL: ISLE 0x410064
BOOL g_reqEnableRMDevice = FALSE;

// STRING: ISLE 0x4101dc
#define WINDOW_TITLE "LEGO®"

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
	m_unk0x24 = 0;
	m_drawCursor = FALSE;
	m_use3dSound = TRUE;
	m_useMusic = TRUE;
	m_useJoystick = FALSE;
	m_joystickIndex = 0;
	m_wideViewAngle = TRUE;
	m_islandQuality = 1;
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

	LegoOmni::CreateInstance();
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
}

// FUNCTION: ISLE 0x401260
void IsleApp::Close()
{
	MxDSAction ds;
	ds.SetUnknown24(-2);

	if (Lego()) {
		GameState()->Save(0);
		if (InputManager()) {
			InputManager()->QueueEvent(c_notificationKeyPress, 0, 0, 0, VK_SPACE);
		}

		VideoManager()->Get3DManager()->GetLego3DView()->GetViewManager()->RemoveAll(NULL);

		Lego()->RemoveWorld(ds.GetAtomId(), ds.GetObjectId());
		Lego()->DeleteObject(ds);
		TransitionManager()->SetWaitIndicator(NULL);
		Lego()->StopTimer();

		MxLong lVar8;
		do {
			lVar8 = Streamer()->Close(NULL);
		} while (lVar8 == 0);

		while (Lego()) {
			if (Lego()->DoesEntityExist(ds)) {
				break;
			}

			Timer()->GetRealTime();
			TickleManager()->Tickle();
		}
	}
}

// FUNCTION: ISLE 0x4013b0
BOOL IsleApp::SetupLegoOmni()
{
	BOOL result = FALSE;
	char mediaPath[256];
	GetProfileStringA("LEGO Island", "MediaPath", "", mediaPath, sizeof(mediaPath));

	// [library:window] For now, get the underlying Windows HWND to pass into Omni
	HWND hwnd =
		(HWND) SDL_GetProperty(SDL_GetWindowProperties(m_windowHandle), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

#ifdef COMPAT_MODE
	BOOL failure;
	{
		MxOmniCreateParam param(mediaPath, (struct HWND__*) hwnd, m_videoParam, MxOmniCreateFlags());
		failure = Lego()->Create(param) == FAILURE;
	}
#else
	BOOL failure = Lego()->Create(MxOmniCreateParam(mediaPath, (struct HWND__*) hwnd, m_videoParam, MxOmniCreateFlags())
				   ) == FAILURE;
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
	BOOL fullScreen,
	BOOL flipSurfaces,
	BOOL backBuffers,
	BOOL using8bit,
	BOOL using16bit,
	BOOL param_6,
	BOOL param_7,
	BOOL wideViewAngle,
	char* deviceId
)
{
	m_videoParam.Flags().SetFullScreen(fullScreen);
	m_videoParam.Flags().SetFlipSurfaces(flipSurfaces);
	m_videoParam.Flags().SetBackBuffers(!backBuffers);
	m_videoParam.Flags().SetF2bit0(!param_6);
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

int SDL_AppInit(void** appstate, int argc, char** argv)
{
	*appstate = NULL;

	// Add subsystems as necessary later
	if (SDL_Init(SDL_INIT_VIDEO) != 0 || SDL_Init(SDL_INIT_TIMER) != 0) {
		SDL_ShowSimpleMessageBox(
			SDL_MESSAGEBOX_ERROR,
			"LEGO® Island Error",
			"\"LEGO® Island\" failed to start.  Please quit all other applications and try again.",
			NULL
		);
		return -1;
	}

	// [library:window]
	// Original game checks for an existing instance here.
	// We don't really need that.

	// Create global app instance
	g_isle = new IsleApp();

	// Create window
	if (g_isle->SetupWindow() != SUCCESS) {
		SDL_ShowSimpleMessageBox(
			SDL_MESSAGEBOX_ERROR,
			"LEGO® Island Error",
			"\"LEGO® Island\" failed to start.  Please quit all other applications and try again.",
			NULL
		);
		return -1;
	}

	// Get reference to window
	*appstate = g_isle->GetWindowHandle();
	return 0;
}

int SDL_AppIterate(void* appstate)
{
	if (g_closed) {
		return 1;
	}

	g_isle->Tick(0);

	if (!g_closed) {
		if (g_reqEnableRMDevice) {
			g_reqEnableRMDevice = FALSE;
			VideoManager()->EnableRMDevice();
			g_rmDisabled = FALSE;
			Lego()->StopTimer();
		}

		if (g_closed) {
			return 1;
		}

		if (g_mousedown && g_mousemoved && g_isle) {
			g_isle->Tick(0);
		}

		if (g_mousemoved) {
			g_mousemoved = FALSE;
		}
	}

	return 0;
}

int SDL_AppEvent(void* appstate, const SDL_Event* event)
{
	if (!g_isle) {
		return 0;
	}

	// [library:window] Remaining functionality to be implemented:
	// Full screen - crashes when minimizing/maximizing
	// WM_TIMER - use SDL_Timer functionality instead
	// WM_SETCURSOR - update cursor
	// 0x5400 - custom LEGO Island SetupCursor event

	switch (event->type) {
	case SDL_EVENT_WINDOW_FOCUS_GAINED:
		g_isle->SetWindowActive(TRUE);
		break;
	case SDL_EVENT_WINDOW_FOCUS_LOST:
		g_isle->SetWindowActive(FALSE);
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

		SDL_Keycode keyCode = event->key.keysym.sym;
		if (InputManager()) {
			InputManager()->QueueEvent(c_notificationKeyPress, keyCode, 0, 0, keyCode);
		}
		break;
	}
	case SDL_EVENT_MOUSE_MOTION:
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
			VideoManager()->MoveCursor(Min((int) event->motion.x, 639), Min((int) event->motion.y, 479));
		}
		break;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
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
	case SDL_EVENT_MOUSE_BUTTON_UP:
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
	}

	return 0;
}

void SDL_AppQuit(void* appstate)
{
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
	LoadConfig();
	SetupVideoFlags(
		m_fullScreen,
		m_flipSurfaces,
		m_backBuffersInVram,
		m_using8bit,
		m_using16bit,
		m_unk0x24,
		FALSE,
		m_wideViewAngle,
		m_deviceId
	);

	MxOmni::SetSound3D(m_use3dSound);

	srand(time(NULL));

	if (m_fullScreen) {
		m_windowHandle = SDL_CreateWindow(WINDOW_TITLE, g_windowRect.right, g_windowRect.bottom, SDL_WINDOW_FULLSCREEN);
	}
	else {
		m_windowHandle = SDL_CreateWindow(WINDOW_TITLE, g_windowRect.right, g_windowRect.bottom, 0);
	}

	if (!m_windowHandle) {
		return FAILURE;
	}

	if (!SetupLegoOmni()) {
		return FAILURE;
	}

	GameState()->SetSavePath(m_savePath);
	GameState()->SerializePlayersInfo(1);
	GameState()->SerializeScoreHistory(1);

	int iVar10;
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

	int uVar1 = (m_islandTexture == 0);
	LegoModelPresenter::configureLegoModelPresenter(uVar1);
	LegoPartPresenter::configureLegoPartPresenter(uVar1, iVar10);
	LegoWorldPresenter::configureLegoWorldPresenter(m_islandQuality);
	LegoBuildingManager::configureLegoBuildingManager(m_islandQuality);
	LegoROI::configureLegoROI(iVar10);
	LegoAnimationManager::configureLegoAnimationManager(m_islandQuality);
	if (LegoOmni::GetInstance()) {
		if (LegoOmni::GetInstance()->GetInputManager()) {
			LegoOmni::GetInstance()->GetInputManager()->SetUseJoystick(m_useJoystick);
			LegoOmni::GetInstance()->GetInputManager()->SetJoystickIndex(m_joystickIndex);
		}
	}

	return SUCCESS;
}

// FUNCTION: ISLE 0x4028d0
void IsleApp::LoadConfig()
{
	char* basePath = SDL_GetBasePath();
	char* prefPath = SDL_GetPrefPath("isledecomp", "isle");
	char* iniConfig = new char[strlen(prefPath) + strlen("isle.ini") + 1]();
	strcat(iniConfig, prefPath);
	strcat(iniConfig, "isle.ini");
	dictionary* dict = iniparser_load(iniConfig);

	const char* hdPath = iniparser_getstring(dict, "isle:diskpath", basePath);
	m_hdPath = new char[strlen(hdPath) + 1];
	strcpy(m_hdPath, hdPath);
	MxOmni::SetHD(m_hdPath);

	const char* cdPath = iniparser_getstring(dict, "isle:cdpath", MxOmni::GetCD());
	m_cdPath = new char[strlen(cdPath) + 1];
	strcpy(m_cdPath, cdPath);
	MxOmni::SetCD(m_cdPath);

	m_flipSurfaces = iniparser_getboolean(dict, "isle:Flip Surfaces", m_flipSurfaces);
	m_fullScreen = iniparser_getboolean(dict, "isle:Full Screen", m_fullScreen);
	m_wideViewAngle = iniparser_getboolean(dict, "isle:Wide View Angle", m_wideViewAngle);
	m_use3dSound = iniparser_getboolean(dict, "isle:3DSound", m_use3dSound);
	m_useMusic = iniparser_getboolean(dict, "isle:Music", m_useMusic);
	m_useJoystick = iniparser_getboolean(dict, "isle:UseJoystick", m_useJoystick);
	m_joystickIndex = iniparser_getint(dict, "isle:JoystickIndex", m_joystickIndex);
	m_drawCursor = iniparser_getboolean(dict, "isle:Draw Cursor", m_drawCursor);

	int backBuffersInVRAM = iniparser_getboolean(dict, "isle:Back Buffers in Video RAM", -1);
	if (backBuffersInVRAM != -1) {
		m_backBuffersInVram = !backBuffersInVRAM;
	}

	int bitDepth = iniparser_getint(dict, "isle:Display Bit Depth", -1);
	if (bitDepth != -1) {
		if (bitDepth == 8) {
			m_using8bit = TRUE;
		}
		else if (bitDepth == 16) {
			m_using16bit = TRUE;
		}
	}

	m_islandQuality = iniparser_getint(dict, "isle:Island Quality", 1);
	m_islandTexture = iniparser_getint(dict, "isle:Island Texture", 1);

	const char* deviceId = iniparser_getstring(dict, "isle:3D Device ID", NULL);
	if (deviceId != NULL) {
		m_deviceId = new char[strlen(deviceId) + 1];
		strcpy(m_deviceId, deviceId);
	}

	// [library:config]
	// The original game does not save any data if no savepath is given.
	// Instead, we use SDLs prefPath as a default fallback and always save data.
	const char* savePath = iniparser_getstring(dict, "isle:savepath", prefPath);
	m_savePath = new char[strlen(savePath) + 1];
	strcpy(m_savePath, savePath);

	iniparser_freedict(dict);
	delete[] iniConfig;
	SDL_free(prefPath);
	SDL_free(basePath);
}

// FUNCTION: ISLE 0x402c20
inline void IsleApp::Tick(BOOL sleepIfNotNextFrame)
{
	// GLOBAL: ISLE 0x4101c0
	static MxLong g_lastFrameTime = 0;

	// GLOBAL: ISLE 0x4101bc
	static int g_startupDelay = 200;

	if (!m_windowActive) {
		Sleep(0);
		return;
	}

	if (!Lego()) {
		return;
	}
	if (!TickleManager()) {
		return;
	}
	if (!Timer()) {
		return;
	}

	MxLong currentTime = Timer()->GetRealTime();
	if (currentTime < g_lastFrameTime) {
		g_lastFrameTime = -m_frameDelta;
	}

	if (m_frameDelta + g_lastFrameTime < currentTime) {
		if (!Lego()->IsTimerRunning()) {
			TickleManager()->Tickle();
		}
		g_lastFrameTime = currentTime;

		if (g_startupDelay == 0) {
			return;
		}

		g_startupDelay--;
		if (g_startupDelay != 0) {
			return;
		}

		LegoOmni::GetInstance()->CreateBackgroundAudio();
		BackgroundAudioManager()->Enable(this->m_useMusic);

		MxStreamController* stream = Streamer()->Open("\\lego\\scripts\\isle\\isle", MxStreamer::e_diskStream);
		MxDSAction ds;

		if (!stream) {
			stream = Streamer()->Open("\\lego\\scripts\\nocd", MxStreamer::e_diskStream);
			if (!stream) {
				return;
			}

			ds.SetAtomId(stream->GetAtom());
			ds.SetUnknown24(-1);
			ds.SetObjectId(0);
			VideoManager()->EnableFullScreenMovie(TRUE, TRUE);

			if (Start(&ds) != SUCCESS) {
				return;
			}
		}
		else {
			ds.SetAtomId(stream->GetAtom());
			ds.SetUnknown24(-1);
			ds.SetObjectId(0);
			if (Start(&ds) != SUCCESS) {
				return;
			}
			m_gameStarted = TRUE;
		}
	}
	else if (sleepIfNotNextFrame != 0) {
		Sleep(0);
	}
}

// FUNCTION: ISLE 0x402e80
void IsleApp::SetupCursor(WPARAM wParam)
{
	switch (wParam) {
	case 0:
		m_cursorCurrent = m_cursorArrow;
		break;
	case 1:
		m_cursorCurrent = m_cursorBusy;
		break;
	case 2:
		m_cursorCurrent = m_cursorNo;
		break;
	case 0xB:
		m_cursorCurrent = NULL;
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 0xA:
		break;
	}

	SetCursor(m_cursorCurrent);
}
