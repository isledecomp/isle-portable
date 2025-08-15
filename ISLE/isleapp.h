#ifndef ISLEAPP_H
#define ISLEAPP_H

#include "cursor.h"
#include "lego1_export.h"
#include "legoinputmanager.h"
#include "legoutils.h"
#include "mxtransitionmanager.h"
#include "mxtypes.h"
#include "mxvideoparam.h"

#include <SDL3/SDL.h>
#ifdef MINIWIN
#include "miniwin/windows.h"
#else
#include <windows.h>
#endif
#include "miniwin/miniwindevice.h"

// SIZE 0x8c
class IsleApp {
public:
	IsleApp();
	~IsleApp();

	void Close();

	MxS32 SetupLegoOmni();
	void SetupVideoFlags(
		MxS32 fullScreen,
		MxS32 flipSurfaces,
		MxS32 backBuffers,
		MxS32 using8bit,
		MxS32 using16bit,
		MxS32 param_6,
		MxS32 param_7,
		MxS32 wideViewAngle,
		char* deviceId
	);
	MxResult SetupWindow();

	bool LoadConfig();
	bool Tick();
	void SetupCursor(Cursor p_cursor);

	static MxU8 MapMouseButtonFlagsToModifier(SDL_MouseButtonFlags p_flags);

	HWND GetWindowHandle() { return m_windowHandle; }
	MxLong GetFrameDelta() { return m_frameDelta; }
	MxS32 GetFullScreen() { return m_fullScreen; }
	SDL_Cursor* GetCursorCurrent() { return m_cursorCurrent; }
	SDL_Cursor* GetCursorBusy() { return m_cursorBusy; }
	SDL_Cursor* GetCursorNo() { return m_cursorNo; }
	MxS32 GetDrawCursor() { return m_drawCursor; }
	MxS32 GetGameStarted() { return m_gameStarted; }
	MxFloat GetCursorSensitivity() { return m_cursorSensitivity; }
	LegoInputManager::TouchScheme GetTouchScheme() { return m_touchScheme; }
	MxBool GetHaptic() { return m_haptic; }

	void SetWindowActive(MxS32 p_windowActive) { m_windowActive = p_windowActive; }
	void SetGameStarted(MxS32 p_gameStarted) { m_gameStarted = p_gameStarted; }
	void SetDrawCursor(MxS32 p_drawCursor) { m_drawCursor = p_drawCursor; }

	SDL_AppResult ParseArguments(int argc, char** argv);
	MxResult VerifyFilesystem();
	void DetectGameVersion();
	void MoveVirtualMouseViaJoystick();
	void DetectDoubleTap(const SDL_TouchFingerEvent& p_event);

private:
	char* m_hdPath;              // 0x00
	char* m_cdPath;              // 0x04
	char* m_deviceId;            // 0x08
	char* m_savePath;            // 0x0c
	MxBool m_fullScreen;         // 0x10
	MxS32 m_flipSurfaces;        // 0x14
	MxS32 m_backBuffersInVram;   // 0x18
	MxS32 m_using8bit;           // 0x1c
	MxS32 m_using16bit;          // 0x20
	MxS32 m_hasLightSupport;     // 0x24
	MxS32 m_use3dSound;          // 0x28
	MxS32 m_useMusic;            // 0x2c
	MxS32 m_wideViewAngle;       // 0x38
	MxS32 m_islandQuality;       // 0x3c
	MxS32 m_islandTexture;       // 0x40
	MxS32 m_gameStarted;         // 0x44
	MxLong m_frameDelta;         // 0x48
	MxVideoParam m_videoParam;   // 0x4c
	MxS32 m_windowActive;        // 0x70
	HWND m_windowHandle;         // 0x74
	MxS32 m_drawCursor;          // 0x78
	SDL_Cursor* m_cursorArrow;   // 0x7c
	SDL_Cursor* m_cursorBusy;    // 0x80
	SDL_Cursor* m_cursorNo;      // 0x84
	SDL_Cursor* m_cursorCurrent; // 0x88
	const CursorBitmap* m_cursorArrowBitmap;
	const CursorBitmap* m_cursorBusyBitmap;
	const CursorBitmap* m_cursorNoBitmap;
	const CursorBitmap* m_cursorCurrentBitmap;
	char* m_mediaPath;
	MxFloat m_cursorSensitivity;
	void DisplayArgumentHelp(const char* p_execName);

	const char* m_iniPath;
	MxFloat m_maxLod;
	MxU32 m_maxAllowedExtras;
	MxTransitionManager::TransitionType m_transitionType;
	LegoInputManager::TouchScheme m_touchScheme;
	MxBool m_haptic;
	MxS32 m_xRes;
	MxS32 m_yRes;
	MxS32 m_exclusiveXRes;
	MxS32 m_exclusiveYRes;
	MxFloat m_exclusiveFrameRate;
	MxFloat m_frameRate;
	MxBool m_exclusiveFullScreen;
	MxU32 m_msaaSamples;
	MxFloat m_anisotropic;
};

extern IsleApp* g_isle;
extern MxS32 g_closed;

extern IDirect3DRMMiniwinDevice* GetD3DRMMiniwinDevice();

#endif // ISLEAPP_H
