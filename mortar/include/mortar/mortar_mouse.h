#pragma once

#include "mortar/mortar_video.h"
#include "mortar_begin.h"

#include <stdint.h>

typedef uint32_t MORTAR_MouseID;

#define MORTAR_BUTTON_LEFT 1
#define MORTAR_BUTTON_MIDDLE 2
#define MORTAR_BUTTON_RIGHT 3
#define MORTAR_BUTTON_X1 4
#define MORTAR_BUTTON_X2 5
#define MORTAR_BUTTON_MASK(X) (1u << ((X) -1))

typedef enum MORTAR_MouseButtonFlags {
	MORTAR_BUTTON_LMASK = MORTAR_BUTTON_MASK(MORTAR_BUTTON_LEFT),
	MORTAR_BUTTON_MMASK = MORTAR_BUTTON_MASK(MORTAR_BUTTON_MIDDLE),
	MORTAR_BUTTON_RMASK = MORTAR_BUTTON_MASK(MORTAR_BUTTON_RIGHT),
	MORTAR_BUTTON_X1MASK = MORTAR_BUTTON_MASK(MORTAR_BUTTON_X1),
	MORTAR_BUTTON_X2MASK = MORTAR_BUTTON_MASK(MORTAR_BUTTON_X2),
} MORTAR_MouseButtonFlags;

typedef struct MORTAR_Cursor MORTAR_Cursor;

typedef enum MORTAR_SystemCursor {
	MORTAR_SYSTEM_CURSOR_DEFAULT, /**< Default cursor. Usually an arrow. */
								  //	MORTAR_SYSTEM_CURSOR_TEXT,         /**< Text selection. Usually an I-beam. */
	MORTAR_SYSTEM_CURSOR_WAIT,    /**< Wait. Usually an hourglass or watch or spinning ball. */
								  //	MORTAR_SYSTEM_CURSOR_CROSSHAIR,    /**< Crosshair. */
	//	MORTAR_SYSTEM_CURSOR_PROGRESS,     /**< Program is busy but still interactive. Usually it's WAIT with an arrow.
	//*/ 	MORTAR_SYSTEM_CURSOR_NWSE_RESIZE,  /**< Double arrow pointing northwest and southeast. */
	//	MORTAR_SYSTEM_CURSOR_NESW_RESIZE,  /**< Double arrow pointing northeast and southwest. */
	//	MORTAR_SYSTEM_CURSOR_EW_RESIZE,    /**< Double arrow pointing west and east. */
	//	MORTAR_SYSTEM_CURSOR_NS_RESIZE,    /**< Double arrow pointing north and south. */
	//	MORTAR_SYSTEM_CURSOR_MOVE,         /**< Four pointed arrow pointing north, south, east, and west. */
	MORTAR_SYSTEM_CURSOR_NOT_ALLOWED, /**< Not permitted. Usually a slashed circle or crossbones. */
	//	MORTAR_SYSTEM_CURSOR_POINTER,      /**< Pointer that indicates a link. Usually a pointing hand. */
	//	MORTAR_SYSTEM_CURSOR_NW_RESIZE,    /**< Window resize top-left. This may be a single arrow or a double arrow
	// like NWSE_RESIZE. */ 	MORTAR_SYSTEM_CURSOR_N_RESIZE,     /**< Window resize top. May be NS_RESIZE. */
	//	MORTAR_SYSTEM_CURSOR_NE_RESIZE,    /**< Window resize top-right. May be NESW_RESIZE. */
	//	MORTAR_SYSTEM_CURSOR_E_RESIZE,     /**< Window resize right. May be EW_RESIZE. */
	//	MORTAR_SYSTEM_CURSOR_SE_RESIZE,    /**< Window resize bottom-right. May be NWSE_RESIZE. */
	//	MORTAR_SYSTEM_CURSOR_S_RESIZE,     /**< Window resize bottom. May be NS_RESIZE. */
	//	MORTAR_SYSTEM_CURSOR_SW_RESIZE,    /**< Window resize bottom-left. May be NESW_RESIZE. */
	//	MORTAR_SYSTEM_CURSOR_W_RESIZE,     /**< Window resize left. May be EW_RESIZE. */
	//	MORTAR_SYSTEM_CURSOR_COUNT
} MORTAR_SystemCursor;

extern MORTAR_DECLSPEC bool MORTAR_ShowCursor(void);

extern MORTAR_DECLSPEC bool MORTAR_HideCursor(void);

extern MORTAR_DECLSPEC void MORTAR_WarpMouseInWindow(MORTAR_Window* window, float x, float y);

extern MORTAR_DECLSPEC bool MORTAR_SetCursor(MORTAR_Cursor* cursor);

extern MORTAR_DECLSPEC MORTAR_Cursor* MORTAR_CreateSystemCursor(MORTAR_SystemCursor id);

extern MORTAR_DECLSPEC MORTAR_MouseButtonFlags MORTAR_GetMouseState(float* x, float* y);

#include "mortar_end.h"

MORTAR_ENUM_FLAG_OPERATORS(MORTAR_MouseButtonFlags)
