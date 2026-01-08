#pragma once

#include "mortar/mortar_joystick.h"
#include "mortar/mortar_keyboard.h"
#include "mortar/mortar_keycode.h"
#include "mortar/mortar_mouse.h"
#include "mortar/mortar_touch.h"
#include "mortar_begin.h"

typedef enum MORTAR_EventType {
	MORTAR_EVENT_QUIT = 0x100,

	MORTAR_EVENT_TERMINATING,
#if 0
	MORTAR_EVENT_LOW_MEMORY,
	MORTAR_EVENT_WILL_ENTER_BACKGROUND,
	MORTAR_EVENT_DID_ENTER_BACKGROUND,
	MORTAR_EVENT_WILL_ENTER_FOREGROUND,
	MORTAR_EVENT_DID_ENTER_FOREGROUND,

	MORTAR_EVENT_LOCALE_CHANGED,

	MORTAR_EVENT_SYSTEM_THEME_CHANGED,
	MORTAR_EVENT_DISPLAY_ORIENTATION = 0x151,
	MORTAR_EVENT_DISPLAY_ADDED,
	MORTAR_EVENT_DISPLAY_REMOVED,
	MORTAR_EVENT_DISPLAY_MOVED,
	MORTAR_EVENT_DISPLAY_DESKTOP_MODE_CHANGED,
	MORTAR_EVENT_DISPLAY_CURRENT_MODE_CHANGED,
	MORTAR_EVENT_DISPLAY_CONTENT_SCALE_CHANGED,
	MORTAR_EVENT_DISPLAY_USABLE_BOUNDS_CHANGED,
	MORTAR_EVENT_DISPLAY_FIRST = MORTAR_EVENT_DISPLAY_ORIENTATION,
	MORTAR_EVENT_DISPLAY_LAST = MORTAR_EVENT_DISPLAY_USABLE_BOUNDS_CHANGED,

	MORTAR_EVENT_WINDOW_SHOWN = 0x202,
	MORTAR_EVENT_WINDOW_HIDDEN,
	MORTAR_EVENT_WINDOW_EXPOSED,
	MORTAR_EVENT_WINDOW_MOVED,
	MORTAR_EVENT_WINDOW_RESIZED,
#endif
	MORTAR_EVENT_WINDOW_PIXEL_SIZE_CHANGED,
#if 0
	MORTAR_EVENT_WINDOW_METAL_VIEW_RESIZED,
	MORTAR_EVENT_WINDOW_MINIMIZED,
	MORTAR_EVENT_WINDOW_MAXIMIZED,
	MORTAR_EVENT_WINDOW_RESTORED,
	MORTAR_EVENT_WINDOW_MOUSE_ENTER,
	MORTAR_EVENT_WINDOW_MOUSE_LEAVE,
#endif
	MORTAR_EVENT_WINDOW_FOCUS_GAINED,
	MORTAR_EVENT_WINDOW_FOCUS_LOST,
	MORTAR_EVENT_WINDOW_CLOSE_REQUESTED,
#if 0
	MORTAR_EVENT_WINDOW_HIT_TEST,
	MORTAR_EVENT_WINDOW_ICCPROF_CHANGED,
	MORTAR_EVENT_WINDOW_DISPLAY_CHANGED,
	MORTAR_EVENT_WINDOW_DISPLAY_SCALE_CHANGED,
	MORTAR_EVENT_WINDOW_SAFE_AREA_CHANGED,
	MORTAR_EVENT_WINDOW_OCCLUDED,
	MORTAR_EVENT_WINDOW_ENTER_FULLSCREEN,
	MORTAR_EVENT_WINDOW_LEAVE_FULLSCREEN,
	MORTAR_EVENT_WINDOW_DESTROYED,
	MORTAR_EVENT_WINDOW_HDR_STATE_CHANGED,
	MORTAR_EVENT_WINDOW_FIRST = MORTAR_EVENT_WINDOW_SHOWN,
	MORTAR_EVENT_WINDOW_LAST = MORTAR_EVENT_WINDOW_HDR_STATE_CHANGED,
#endif
	MORTAR_EVENT_KEY_DOWN, //        = 0x300,
	MORTAR_EVENT_KEY_UP,
#if 0
	MORTAR_EVENT_TEXT_EDITING,
	MORTAR_EVENT_TEXT_INPUT,
	MORTAR_EVENT_KEYMAP_CHANGED,
#endif
	MORTAR_EVENT_KEYBOARD_ADDED,
	MORTAR_EVENT_KEYBOARD_REMOVED,
#if 0
	MORTAR_EVENT_TEXT_EDITING_CANDIDATES,
	MORTAR_EVENT_SCREEN_KEYBOARD_SHOWN,
	MORTAR_EVENT_SCREEN_KEYBOARD_HIDDEN,
#endif

	MORTAR_EVENT_MOUSE_MOTION,
	MORTAR_EVENT_MOUSE_BUTTON_DOWN,
	MORTAR_EVENT_MOUSE_BUTTON_UP,
#if 0
	MORTAR_EVENT_MOUSE_WHEEL,
#endif
	MORTAR_EVENT_MOUSE_ADDED,
	MORTAR_EVENT_MOUSE_REMOVED,

#if 0
	MORTAR_EVENT_JOYSTICK_AXIS_MOTION  = 0x600,
	MORTAR_EVENT_JOYSTICK_BALL_MOTION,
	MORTAR_EVENT_JOYSTICK_HAT_MOTION,
	MORTAR_EVENT_JOYSTICK_BUTTON_DOWN,
	MORTAR_EVENT_JOYSTICK_BUTTON_UP,
	MORTAR_EVENT_JOYSTICK_ADDED,
	MORTAR_EVENT_JOYSTICK_REMOVED,
	MORTAR_EVENT_JOYSTICK_BATTERY_UPDATED,
	MORTAR_EVENT_JOYSTICK_UPDATE_COMPLETE,
#endif

	MORTAR_EVENT_GAMEPAD_AXIS_MOTION, //  = 0x650,
	MORTAR_EVENT_GAMEPAD_BUTTON_DOWN,
	MORTAR_EVENT_GAMEPAD_BUTTON_UP,
	MORTAR_EVENT_GAMEPAD_ADDED,
	MORTAR_EVENT_GAMEPAD_REMOVED,
#if 0
		MORTAR_EVENT_GAMEPAD_REMAPPED,
		MORTAR_EVENT_GAMEPAD_TOUCHPAD_DOWN,
		MORTAR_EVENT_GAMEPAD_TOUCHPAD_MOTION,
		MORTAR_EVENT_GAMEPAD_TOUCHPAD_UP,
		MORTAR_EVENT_GAMEPAD_SENSOR_UPDATE,
		MORTAR_EVENT_GAMEPAD_UPDATE_COMPLETE,
		MORTAR_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED,
#endif

	MORTAR_EVENT_FINGER_DOWN,
	MORTAR_EVENT_FINGER_UP,
	MORTAR_EVENT_FINGER_MOTION,
	MORTAR_EVENT_FINGER_CANCELED,

#if 0
	MORTAR_EVENT_PINCH_BEGIN      = 0x710,
	MORTAR_EVENT_PINCH_UPDATE,
	MORTAR_EVENT_PINCH_END,

	MORTAR_EVENT_CLIPBOARD_UPDATE = 0x900,

	MORTAR_EVENT_DROP_FILE        = 0x1000,
	MORTAR_EVENT_DROP_TEXT,
	MORTAR_EVENT_DROP_BEGIN,
	MORTAR_EVENT_DROP_COMPLETE,
	MORTAR_EVENT_DROP_POSITION,

	MORTAR_EVENT_AUDIO_DEVICE_ADDED = 0x1100,
	MORTAR_EVENT_AUDIO_DEVICE_REMOVED,
	MORTAR_EVENT_AUDIO_DEVICE_FORMAT_CHANGED,

	MORTAR_EVENT_SENSOR_UPDATE = 0x1200,

	MORTAR_EVENT_PEN_PROXIMITY_IN = 0x1300,
	MORTAR_EVENT_PEN_PROXIMITY_OUT,
	MORTAR_EVENT_PEN_DOWN,
	MORTAR_EVENT_PEN_UP,
	MORTAR_EVENT_PEN_BUTTON_DOWN,
	MORTAR_EVENT_PEN_BUTTON_UP,
	MORTAR_EVENT_PEN_MOTION,
	MORTAR_EVENT_PEN_AXIS,

	MORTAR_EVENT_CAMERA_DEVICE_ADDED = 0x1400,
	MORTAR_EVENT_CAMERA_DEVICE_REMOVED,
	MORTAR_EVENT_CAMERA_DEVICE_APPROVED,
	MORTAR_EVENT_CAMERA_DEVICE_DENIED,

	MORTAR_EVENT_RENDER_TARGETS_RESET = 0x2000,
	MORTAR_EVENT_RENDER_DEVICE_RESET,
	MORTAR_EVENT_RENDER_DEVICE_LOST,

	MORTAR_EVENT_PRIVATE0 = 0x4000,
	MORTAR_EVENT_PRIVATE1,
	MORTAR_EVENT_PRIVATE2,
	MORTAR_EVENT_PRIVATE3,

	MORTAR_EVENT_POLL_SENTINEL = 0x7F00,
#endif

	MORTAR_EVENT_USER = 0x8000,

#if 0
		MORTAR_EVENT_LAST    = 0xFFFF,

		MORTAR_EVENT_ENUM_PADDING = 0x7FFFFFFF
#endif

} MORTAR_EventType;

typedef struct MORTAR_CommonEvent {
	MORTAR_EventType type;
	uint32_t reserved;
	uint64_t timestamp; /**< In nanoseconds, populated using MORTAR_GetTicksNS() */
} MORTAR_CommonEvent;

typedef struct MORTAR_WindowEvent {
	MORTAR_EventType type; /**< MORTAR_EVENT_WINDOW_* */
	uint32_t reserved;
	uint64_t timestamp; /**< In nanoseconds, populated using MORTAR_GetTicksNS() */
#if 0
	MORTAR_WindowID windowID; /**< The associated window */
	int32_t data1;       /**< event dependent data */
	int32_t data2;       /**< event dependent data */
#endif
} MORTAR_WindowEvent;

typedef struct MORTAR_KeyboardDeviceEvent {
	MORTAR_EventType type; /**< MORTAR_EVENT_KEYBOARD_ADDED or MORTAR_EVENT_KEYBOARD_REMOVED */
	uint32_t reserved;
	uint64_t timestamp;      /**< In nanoseconds, populated using MORTAR_GetTicksNS() */
	MORTAR_KeyboardID which; /**< The keyboard instance id */
} MORTAR_KeyboardDeviceEvent;

typedef struct MORTAR_KeyboardEvent {
	MORTAR_EventType type; /**< MORTAR_EVENT_KEY_DOWN or MORTAR_EVENT_KEY_UP */
	uint32_t reserved;
	uint64_t timestamp; /**< In nanoseconds, populated using MORTAR_GetTicksNS() */
#if 0
	 MORTAR_WindowID windowID;  /**< The window with keyboard focus, if any */
#endif
	MORTAR_KeyboardID which; /**< The keyboard instance id, or 0 if unknown or virtual */
#if 0
	 MORTAR_Scancode scancode;  /**< SDL physical key code */
#endif
	MORTAR_Keycode key; /**< SDL virtual key code */
	MORTAR_Keymod mod;  /**< current key modifiers */
#if 0
	 uint16_t raw;             /**< The platform dependent scancode for this event */
	 bool down;              /**< true if the key is pressed */
#endif
	bool repeat; /**< true if this is a key repeat */
} MORTAR_KeyboardEvent;

typedef struct MORTAR_MouseDeviceEvent {
	MORTAR_EventType type; /**< MORTAR_EVENT_MOUSE_ADDED or MORTAR_EVENT_MOUSE_REMOVED */
	uint32_t reserved;
	uint64_t timestamp;   /**< In nanoseconds, populated using MORTAR_GetTicksNS() */
	MORTAR_MouseID which; /**< The mouse instance id */
} MORTAR_MouseDeviceEvent;

typedef struct MORTAR_MouseMotionEvent {
	MORTAR_EventType type; /**< MORTAR_EVENT_MOUSE_MOTION */
	uint32_t reserved;
	uint64_t timestamp;
#if 0
	MORTAR_WindowID windowID; /**< The window with mouse focus, if any */
#endif
	MORTAR_MouseID which; /**< The mouse instance id in relative mode, MORTAR_TOUCH_MOUSEID for touch events, or 0 */
	MORTAR_MouseButtonFlags state; /**< The current button state */
	float x;
	float y;
#if 0
	float xrel;
	float yrel;
#endif
} MORTAR_MouseMotionEvent;

typedef struct MORTAR_MouseButtonEvent {
	MORTAR_EventType type;
	uint32_t reserved;
	uint64_t timestamp; /**< In nanoseconds, populated using MORTAR_GetTicksNS() */
#if 0
	MORTAR_WindowID windowID; /**< The window with mouse focus, if any */
#endif
	MORTAR_MouseID which; /**< The mouse instance id in relative mode, MORTAR_TOUCH_MOUSEID for touch events, or 0 */
#if 0
	uint8_t button;       /**< The mouse button index */
	bool down;          /**< true if the button is pressed */
	uint8_t clicks;       /**< 1 for single-click, 2 for double-click, etc. */
	uint8_t padding;
#endif
	float x;
	float y;
} MORTAR_MouseButtonEvent;

typedef struct MORTAR_JoyDeviceEvent {
	MORTAR_EventType type; /**< MORTAR_EVENT_JOYSTICK_ADDED or MORTAR_EVENT_JOYSTICK_REMOVED or
							  MORTAR_EVENT_JOYSTICK_UPDATE_COMPLETE */
	uint32_t reserved;
	uint64_t timestamp;      /**< In nanoseconds, populated using MORTAR_GetTicksNS() */
	MORTAR_JoystickID which; /**< The joystick instance id */
} MORTAR_JoyDeviceEvent;

typedef struct MORTAR_GamepadAxisEvent {
	MORTAR_EventType type; /**< MORTAR_EVENT_GAMEPAD_AXIS_MOTION */
	uint32_t reserved;
	uint64_t timestamp;      /**< In nanoseconds, populated using MORTAR_GetTicksNS() */
	MORTAR_JoystickID which; /**< The joystick instance id */
	uint8_t axis;            /**< The gamepad axis (MORTAR_GamepadAxis) */
#if 0
	uint8_t padding1;
	uint8_t padding2;
	uint8_t padding3;
#endif
	int16_t value; /**< The axis value (range: -32768 to 32767) */
				   //	uint16_t padding4;
} MORTAR_GamepadAxisEvent;

typedef struct MORTAR_GamepadButtonEvent {
	MORTAR_EventType type; /**< MORTAR_EVENT_GAMEPAD_BUTTON_DOWN or MORTAR_EVENT_GAMEPAD_BUTTON_UP */
	uint32_t reserved;
	uint64_t timestamp;      /**< In nanoseconds, populated using MORTAR_GetTicksNS() */
	MORTAR_JoystickID which; /**< The joystick instance id */
	uint8_t button;          /**< The gamepad button (MORTAR_GamepadButton) */
#if 0
	bool down;               /**< true if the button is pressed */
	uint8_t padding1;
	uint8_t padding2;
#endif
} MORTAR_GamepadButtonEvent;

typedef struct MORTAR_QuitEvent {
	MORTAR_EventType type;
	uint32_t reserved;
	uint64_t timestamp;
} MORTAR_QuitEvent;

typedef struct MORTAR_UserEvent {
	uint32_t type; /**< MORTAR_EVENT_USER through MORTAR_EVENT_LAST, uint32_t because these are not in the
					  MORTAR_EventType enumeration */
	uint32_t reserved;
	uint64_t timestamp; /**< In nanoseconds, populated using MORTAR_GetTicksNS() */
#if 0
	MORTAR_WindowID windowID; /**< The associated window if any */
#endif
	int32_t code; /**< User defined event code */
	void* data1;  /**< User defined data pointer */
	void* data2;  /**< User defined data pointer */
} MORTAR_UserEvent;

typedef struct MORTAR_TouchFingerEvent {
	MORTAR_EventType type;
	uint32_t reserved;
	uint64_t timestamp;     /**< In nanoseconds, populated using MORTAR_GetTicksNS() */
	MORTAR_TouchID touchID; /**< The touch device id */
	MORTAR_FingerID fingerID;
	float x; /**< Normalized in the range 0...1 */
	float y; /**< Normalized in the range 0...1 */
#if 0
	float dx;           /**< Normalized in the range -1...1 */
	float dy;           /**< Normalized in the range -1...1 */
	float pressure;     /**< Normalized in the range 0...1 */
	MORTAR_WindowID windowID; /**< The window underneath the finger, if any */
#endif
} MORTAR_TouchFingerEvent;

typedef union MORTAR_Event {
	MORTAR_EventType type;
	MORTAR_CommonEvent common;
#if 0
	MORTAR_DisplayEvent display;
#endif
	MORTAR_WindowEvent window;
	MORTAR_KeyboardDeviceEvent kdevice;
	MORTAR_KeyboardEvent key;
#if 0
	MORTAR_TextEditingEvent edit;
	MORTAR_TextEditingCandidatesEvent edit_candidates;
	MORTAR_TextInputEvent text;
#endif
	MORTAR_MouseDeviceEvent mdevice;
	MORTAR_MouseMotionEvent motion;
	MORTAR_MouseButtonEvent button;
#if 0
	MORTAR_MouseWheelEvent wheel;
#endif
	MORTAR_JoyDeviceEvent jdevice;
#if 0
	MORTAR_JoyAxisEvent jaxis;
	MORTAR_JoyBallEvent jball;
	MORTAR_JoyHatEvent jhat;
	MORTAR_JoyButtonEvent jbutton;
	MORTAR_JoyBatteryEvent jbattery;
	MORTAR_GamepadDeviceEvent gdevice;
#endif
	MORTAR_GamepadAxisEvent gaxis;
	MORTAR_GamepadButtonEvent gbutton;
#if 0
	MORTAR_GamepadTouchpadEvent gtouchpad;
	MORTAR_GamepadSensorEvent gsensor;
	MORTAR_AudioDeviceEvent adevice;
	MORTAR_CameraDeviceEvent cdevice;
	MORTAR_SensorEvent sensor;
#endif
	MORTAR_QuitEvent quit;
	MORTAR_UserEvent user;
	MORTAR_TouchFingerEvent tfinger;
#if 0
	MORTAR_PinchFingerEvent pinch;
	MORTAR_PenProximityEvent pproximity;
	MORTAR_PenTouchEvent ptouch;
	MORTAR_PenMotionEvent pmotion;
	MORTAR_PenButtonEvent pbutton;
	MORTAR_PenAxisEvent paxis;
	MORTAR_RenderEvent render;
	MORTAR_DropEvent drop;
	MORTAR_ClipboardEvent clipboard;
#endif
} MORTAR_Event;

typedef bool (*MORTAR_EventFilter)(void* userdata, MORTAR_Event* event);

extern MORTAR_DECLSPEC bool MORTAR_PushEvent(MORTAR_Event* event);

extern MORTAR_DECLSPEC uint32_t MORTAR_RegisterEvents(int numevents);

extern MORTAR_DECLSPEC bool MORTAR_AddEventWatch(MORTAR_EventFilter filter, void* userdata);

#include "mortar_end.h"
