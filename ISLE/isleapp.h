#ifndef ISLEAPP_H
#define ISLEAPP_H

#include "lego/legoomni/include/legoutils.h"
#include "mxtypes.h"
#include "mxvideoparam.h"

#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>
#include <windows.h>

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

	void LoadConfig();
	void Tick();
	void SetupCursor(Cursor p_cursor);

	static MxU8 MapMouseButtonFlagsToModifier(SDL_MouseButtonFlags p_flags);

	inline SDL_Window* GetWindowHandle() { return m_windowHandle; }
	inline MxLong GetFrameDelta() { return m_frameDelta; }
	inline MxS32 GetFullScreen() { return m_fullScreen; }
	inline SDL_Cursor* GetCursorCurrent() { return m_cursorCurrent; }
	inline SDL_Cursor* GetCursorBusy() { return m_cursorBusy; }
	inline SDL_Cursor* GetCursorNo() { return m_cursorNo; }
	inline MxS32 GetDrawCursor() { return m_drawCursor; }

	inline void SetWindowActive(MxS32 p_windowActive) { m_windowActive = p_windowActive; }

private:
	char* m_hdPath;              // 0x00
	char* m_cdPath;              // 0x04
	char* m_deviceId;            // 0x08
	char* m_savePath;            // 0x0c
	MxS32 m_fullScreen;          // 0x10
	MxS32 m_flipSurfaces;        // 0x14
	MxS32 m_backBuffersInVram;   // 0x18
	MxS32 m_using8bit;           // 0x1c
	MxS32 m_using16bit;          // 0x20
	MxS32 m_unk0x24;             // 0x24
	MxS32 m_use3dSound;          // 0x28
	MxS32 m_useMusic;            // 0x2c
	MxS32 m_useJoystick;         // 0x30
	MxS32 m_joystickIndex;       // 0x34
	MxS32 m_wideViewAngle;       // 0x38
	MxS32 m_islandQuality;       // 0x3c
	MxS32 m_islandTexture;       // 0x40
	MxS32 m_gameStarted;         // 0x44
	MxLong m_frameDelta;         // 0x48
	MxVideoParam m_videoParam;   // 0x4c
	MxS32 m_windowActive;        // 0x70
	SDL_Window* m_windowHandle;  // 0x74
	MxS32 m_drawCursor;          // 0x78
	SDL_Cursor* m_cursorArrow;   // 0x7c
	SDL_Cursor* m_cursorBusy;    // 0x80
	SDL_Cursor* m_cursorNo;      // 0x84
	SDL_Cursor* m_cursorCurrent; // 0x88
};

#endif // ISLEAPP_H
