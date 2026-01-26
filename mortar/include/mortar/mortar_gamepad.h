#pragma once

#include "mortar/mortar_joystick.h"
#include "mortar_begin.h"

#include <stdint.h>

typedef struct MORTAR_Gamepad MORTAR_Gamepad;

typedef enum MORTAR_GamepadButton {
	MORTAR_GAMEPAD_BUTTON_INVALID = -1,
	MORTAR_GAMEPAD_BUTTON_SOUTH, /**< Bottom face button (e.g. Xbox A button) */
	MORTAR_GAMEPAD_BUTTON_EAST,  /**< Right face button (e.g. Xbox B button) */
	MORTAR_GAMEPAD_BUTTON_WEST,  /**< Left face button (e.g. Xbox X button) */
	MORTAR_GAMEPAD_BUTTON_NORTH, /**< Top face button (e.g. Xbox Y button) */
	MORTAR_GAMEPAD_BUTTON_BACK,
	MORTAR_GAMEPAD_BUTTON_GUIDE,
	MORTAR_GAMEPAD_BUTTON_START,
	MORTAR_GAMEPAD_BUTTON_LEFT_STICK,
	MORTAR_GAMEPAD_BUTTON_RIGHT_STICK,
	MORTAR_GAMEPAD_BUTTON_LEFT_SHOULDER,
	MORTAR_GAMEPAD_BUTTON_RIGHT_SHOULDER,
	MORTAR_GAMEPAD_BUTTON_DPAD_UP,
	MORTAR_GAMEPAD_BUTTON_DPAD_DOWN,
	MORTAR_GAMEPAD_BUTTON_DPAD_LEFT,
	MORTAR_GAMEPAD_BUTTON_DPAD_RIGHT,
	MORTAR_GAMEPAD_BUTTON_MISC1,         /**< Additional button (e.g. Xbox Series X share button, PS5 microphone button,
											Nintendo Switch Pro capture button, Amazon Luna microphone button, Google Stadia
											capture button) */
	MORTAR_GAMEPAD_BUTTON_RIGHT_PADDLE1, /**< Upper or primary paddle, under your right hand (e.g. Xbox Elite paddle P1,
											DualSense Edge RB button, Right Joy-Con SR button) */
	MORTAR_GAMEPAD_BUTTON_LEFT_PADDLE1,  /**< Upper or primary paddle, under your left hand (e.g. Xbox Elite paddle P3,
											DualSense Edge LB button, Left Joy-Con SL button) */
	MORTAR_GAMEPAD_BUTTON_RIGHT_PADDLE2, /**< Lower or secondary paddle, under your right hand (e.g. Xbox Elite paddle
											P2, DualSense Edge right Fn button, Right Joy-Con SL button) */
	MORTAR_GAMEPAD_BUTTON_LEFT_PADDLE2, /**< Lower or secondary paddle, under your left hand (e.g. Xbox Elite paddle P4,
										   DualSense Edge left Fn button, Left Joy-Con SR button) */
	MORTAR_GAMEPAD_BUTTON_TOUCHPAD,     /**< PS4/PS5 touchpad button */
	MORTAR_GAMEPAD_BUTTON_MISC2,        /**< Additional button */
	MORTAR_GAMEPAD_BUTTON_MISC3,        /**< Additional button (e.g. Nintendo GameCube left trigger click) */
	MORTAR_GAMEPAD_BUTTON_MISC4,        /**< Additional button (e.g. Nintendo GameCube right trigger click) */
	MORTAR_GAMEPAD_BUTTON_MISC5,        /**< Additional button */
	MORTAR_GAMEPAD_BUTTON_MISC6,        /**< Additional button */
	MORTAR_GAMEPAD_BUTTON_COUNT
} MORTAR_GamepadButton;

typedef enum MORTAR_GamepadAxis {
	MORTAR_GAMEPAD_AXIS_INVALID = -1,
	MORTAR_GAMEPAD_AXIS_LEFTX,
	MORTAR_GAMEPAD_AXIS_LEFTY,
	MORTAR_GAMEPAD_AXIS_RIGHTX,
	MORTAR_GAMEPAD_AXIS_RIGHTY,
	//	MORTAR_GAMEPAD_AXIS_LEFT_TRIGGER,
	MORTAR_GAMEPAD_AXIS_RIGHT_TRIGGER,
	//	MORTAR_GAMEPAD_AXIS_COUNT
} MORTAR_GamepadAxis;

extern MORTAR_DECLSPEC MORTAR_Gamepad* MORTAR_OpenGamepad(MORTAR_JoystickID instance_id);

extern MORTAR_DECLSPEC void MORTAR_CloseGamepad(MORTAR_Gamepad* gamepad);

extern MORTAR_DECLSPEC int16_t MORTAR_GetGamepadAxis(MORTAR_Gamepad* gamepad, MORTAR_GamepadAxis axis);

extern MORTAR_DECLSPEC MORTAR_Joystick* MORTAR_GetGamepadJoystick(MORTAR_Gamepad* gamepad);

extern MORTAR_DECLSPEC bool MORTAR_RumbleGamepad(
	MORTAR_Gamepad* gamepad,
	uint16_t low_frequency_rumble,
	uint16_t high_frequency_rumble,
	uint32_t duration_ms
);

#include "mortar_end.h"
