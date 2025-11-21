#pragma once

#include "mortar/mortar_pixels.h"
#include "mortar/mortar_stdinc.h"
#include "mortar/mortar_surface.h"
#include "mortar_begin.h"

typedef struct MORTAR_Window MORTAR_Window;

typedef uint32_t MORTAR_DisplayID;

typedef enum {
	MORTAR_WINDOW_FULLSCREEN = 0x00000001,
	MORTAR_WINDOW_BORDERLESS = 0x00000002,
	MORTAR_WINDOW_RESIZABLE = 0x00000004
} MORTAR_WindowFlags;

typedef struct MORTAR_DisplayMode {
	MORTAR_PixelFormat format;
	int w;
	int h;
	float refresh_rate;
} MORTAR_DisplayMode;

typedef enum MORTAR_WindowProperty {
	MORTAR_WINDOW_PROPERTY_USER,
	MORTAR_WINDOW_PROPERTY_SDL3WINDOW,
	MORTAR_WINDOW_PROPERTY_HWND,
} MORTAR_WindowProperty;

#define MORTAR_GL_CONTEXT_PROFILE_CORE 0x0001
#define MORTAR_GL_CONTEXT_PROFILE_COMPATIBILITY 0x0002
#define MORTAR_GL_CONTEXT_PROFILE_ES 0x0004

typedef enum MORTAR_GLAttr {
	//	MORTAR_GL_RED_SIZE,
	//	MORTAR_GL_GREEN_SIZE,
	//	MORTAR_GL_BLUE_SIZE,
	//	MORTAR_GL_ALPHA_SIZE,
	//	MORTAR_GL_BUFFER_SIZE,
	MORTAR_GL_DOUBLEBUFFER,
	MORTAR_GL_DEPTH_SIZE,
	//	MORTAR_GL_STENCIL_SIZE,
	//	MORTAR_GL_ACCUM_RED_SIZE,
	//	MORTAR_GL_ACCUM_GREEN_SIZE,
	//	MORTAR_GL_ACCUM_BLUE_SIZE,
	//	MORTAR_GL_ACCUM_ALPHA_SIZE,
	//	MORTAR_GL_STEREO,
	MORTAR_GL_MULTISAMPLEBUFFERS,
	MORTAR_GL_MULTISAMPLESAMPLES,
	//	MORTAR_GL_ACCELERATED_VISUAL,
	//	MORTAR_GL_RETAINED_BACKING,
	MORTAR_GL_CONTEXT_MAJOR_VERSION,
	MORTAR_GL_CONTEXT_MINOR_VERSION,
	//	MORTAR_GL_CONTEXT_FLAGS,
	MORTAR_GL_CONTEXT_PROFILE_MASK,
	//	MORTAR_GL_SHARE_WITH_CURRENT_CONTEXT,
	//	MORTAR_GL_FRAMEBUFFER_SRGB_CAPABLE,
	//	MORTAR_GL_CONTEXT_RELEASE_BEHAVIOR,
	//	MORTAR_GL_CONTEXT_RESET_NOTIFICATION,
	//	MORTAR_GL_CONTEXT_NO_ERROR,
	//	MORTAR_GL_FLOATBUFFERS,
	//	MORTAR_GL_EGL_PLATFORM
} MORTAR_GLAttr;

typedef struct MORTAR_EX_CreateWindowProps {
	int width;
	int height;
	bool fullscreen;
	const char* title;
	bool hidden;
	struct {
		bool enabled;
		bool doublebuffer;
		int depth_size;
	} opengl;
} MORTAR_EX_CreateWindowProps;

extern MORTAR_DECLSPEC const char* MORTAR_GetCurrentVideoDriver(void);

extern MORTAR_DECLSPEC bool MORTAR_RaiseWindow(MORTAR_Window* window);

extern MORTAR_DECLSPEC bool MORTAR_SetWindowSize(MORTAR_Window* window, int w, int h);

extern MORTAR_DECLSPEC bool MORTAR_SetWindowPosition(MORTAR_Window* window, int x, int y);

extern MORTAR_DECLSPEC MORTAR_WindowFlags MORTAR_GetWindowFlags(MORTAR_Window* window);

extern MORTAR_DECLSPEC bool MORTAR_SetWindowBordered(MORTAR_Window* window, bool bordered);

extern MORTAR_DECLSPEC bool MORTAR_SetWindowResizable(MORTAR_Window* window, bool resizable);

extern MORTAR_DECLSPEC bool MORTAR_GetWindowSize(MORTAR_Window* window, int* w, int* h);

extern MORTAR_DECLSPEC bool MORTAR_GetWindowSizeInPixels(MORTAR_Window* window, int* w, int* h);

extern MORTAR_DECLSPEC MORTAR_DisplayID MORTAR_GetPrimaryDisplay(void);

extern MORTAR_DECLSPEC MORTAR_DisplayMode** MORTAR_GetFullscreenDisplayModes(MORTAR_DisplayID displayID, int* count);

extern MORTAR_DECLSPEC const MORTAR_DisplayMode* MORTAR_GetCurrentDisplayMode(MORTAR_DisplayID displayID);

extern MORTAR_DECLSPEC bool MORTAR_SetWindowFullscreenMode(MORTAR_Window* window, const MORTAR_DisplayMode* mode);

extern MORTAR_DECLSPEC bool MORTAR_SetWindowFullscreen(MORTAR_Window* window, bool fullscreen);

extern MORTAR_DECLSPEC bool MORTAR_GL_ExtensionSupported(const char* extension);

extern MORTAR_DECLSPEC MORTAR_FunctionPointer MORTAR_GL_GetProcAddress(const char* proc);

extern MORTAR_DECLSPEC void MORTAR_GL_ResetAttributes(void);

extern MORTAR_DECLSPEC bool MORTAR_GL_SetAttribute(MORTAR_GLAttr attr, int value);

extern MORTAR_DECLSPEC bool MORTAR_GL_MakeCurrent(MORTAR_Window* window, MORTAR_GLContext context);

extern MORTAR_DECLSPEC MORTAR_GLContext MORTAR_GL_CreateContext(MORTAR_Window* window);

extern MORTAR_DECLSPEC bool MORTAR_GL_DestroyContext(MORTAR_GLContext context);

extern MORTAR_DECLSPEC bool MORTAR_GL_SwapWindow(MORTAR_Window* window);

extern MORTAR_DECLSPEC bool MORTAR_SetWindowTitle(MORTAR_Window* window, const char* title);

extern MORTAR_DECLSPEC bool MORTAR_SetWindowIcon(MORTAR_Window* window, MORTAR_Surface* icon);

extern MORTAR_DECLSPEC bool MORTAR_GetClosestFullscreenDisplayMode(
	MORTAR_DisplayID displayID,
	int w,
	int h,
	float refresh_rate,
	bool include_high_density_modes,
	MORTAR_DisplayMode* closest
);

extern MORTAR_DECLSPEC MORTAR_DisplayID MORTAR_GetDisplayForWindow(MORTAR_Window* window);

extern MORTAR_DECLSPEC MORTAR_Window* MORTAR_EX_CreateWindow(MORTAR_EX_CreateWindowProps* createProps);

extern MORTAR_DECLSPEC void MORTAR_DestroyWindow(MORTAR_Window* window);

extern MORTAR_DECLSPEC bool MORTAR_EXT_SetWindowProperty(
	MORTAR_Window* window,
	MORTAR_WindowProperty property,
	void* value
);

extern MORTAR_DECLSPEC void* MORTAR_EXT_GetWindowProperty(
	MORTAR_Window* window,
	MORTAR_WindowProperty property,
	void* default_value
);

#include "mortar_end.h"

MORTAR_ENUM_FLAG_OPERATORS(MORTAR_WindowFlags)
