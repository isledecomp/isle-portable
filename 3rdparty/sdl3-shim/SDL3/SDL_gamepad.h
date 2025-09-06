#pragma once

#include <SDL2/SDL_gamecontroller.h>

// https://wiki.libsdl.org/SDL3/README-migration#sdl_gamecontrollerh

#define SDL_Gamepad SDL_GameController

#define SDL_INIT_GAMEPAD SDL_INIT_GAMECONTROLLER

// #define SDL_CloseGamepad SDL_GameControllerClose

//
// #define SDL_GamepadAxis SDL_GameControllerAxis
// #define SDL_GamepadBindingType SDL_GameControllerBindType
// #define SDL_GamepadButton SDL_GameControllerButton
// #define SDL_GamepadType SDL_GameControllerType

// #define SDL_AddGamepadMapping SDL_GameControllerAddMapping
// #define SDL_AddGamepadMappingsFromFile SDL_GameControllerAddMappingsFromFile
// #define SDL_AddGamepadMappingsFromIO SDL_GameControllerAddMappingsFromRW
#define SDL_CloseGamepad SDL_GameControllerClose
// #define SDL_GetGamepadFromID SDL_GameControllerFromInstanceID
// #define SDL_GetGamepadFromPlayerIndex SDL_GameControllerFromPlayerIndex
// #define SDL_GetGamepadAppleSFSymbolsNameForAxis SDL_GameControllerGetAppleSFSymbolsNameForAxis
// #define SDL_GetGamepadAppleSFSymbolsNameForButton SDL_GameControllerGetAppleSFSymbolsNameForButton
// #define SDL_GamepadConnected SDL_GameControllerGetAttached
#define SDL_GetGamepadAxis SDL_GameControllerGetAxis
// #define SDL_GetGamepadAxisFromString SDL_GameControllerGetAxisFromString
// #define SDL_GetGamepadButton SDL_GameControllerGetButton
// #define SDL_GetGamepadButtonFromString SDL_GameControllerGetButtonFromString
// #define SDL_GetGamepadFirmwareVersion SDL_GameControllerGetFirmwareVersion
#define SDL_GetGamepadJoystick SDL_GameControllerGetJoystick
// #define SDL_GetNumGamepadTouchpadFingers SDL_GameControllerGetNumTouchpadFingers
// #define SDL_GetNumGamepadTouchpads SDL_GameControllerGetNumTouchpads
// #define SDL_GetGamepadPlayerIndex SDL_GameControllerGetPlayerIndex
// #define SDL_GetGamepadProduct SDL_GameControllerGetProduct
// #define SDL_GetGamepadProductVersion SDL_GameControllerGetProductVersion
// #define SDL_GetGamepadSensorData SDL_GameControllerGetSensorData, returns bool
// #define SDL_GetGamepadSensorDataRate SDL_GameControllerGetSensorDataRate
// #define SDL_GetGamepadSerial SDL_GameControllerGetSerial
// #define SDL_GetGamepadSteamHandle SDL_GameControllerGetSteamHandle
// #define SDL_GetGamepadStringForAxis SDL_GameControllerGetStringForAxis
// #define SDL_GetGamepadStringForButton SDL_GameControllerGetStringForButton
// #define SDL_GetGamepadTouchpadFinger SDL_GameControllerGetTouchpadFinger, returns bool
// #define SDL_GetGamepadType SDL_GameControllerGetType
// #define SDL_GetGamepadVendor SDL_GameControllerGetVendor
// #define SDL_GamepadHasAxis SDL_GameControllerHasAxis
// #define SDL_GamepadHasButton SDL_GameControllerHasButton
// #define SDL_GamepadHasSensor SDL_GameControllerHasSensor
// #define SDL_GamepadSensorEnabled SDL_GameControllerIsSensorEnabled
// #define SDL_GetGamepadMapping SDL_GameControllerMapping
// #define SDL_GetGamepadMappingForGUID SDL_GameControllerMappingForGUID
// #define SDL_GetGamepadName SDL_GameControllerName
#define SDL_OpenGamepad SDL_GameControllerOpen
// #define SDL_GetGamepadPath SDL_GameControllerPath
#define SDL_RumbleGamepad(...) (SDL_GameControllerRumble(__VA_ARGS__) == 0)
// #define SDL_RumbleGamepadTriggers SDL_GameControllerRumbleTriggers, returns bool
// #define SDL_SendGamepadEffect SDL_GameControllerSendEffect, returns bool
// #define SDL_SetGamepadLED SDL_GameControllerSetLED, returns bool
// #define SDL_SetGamepadPlayerIndex SDL_GameControllerSetPlayerIndex, returns bool
// #define SDL_SetGamepadSensorEnabled SDL_GameControllerSetSensorEnabled, returns bool
// #define SDL_UpdateGamepads SDL_GameControllerUpdate
// #define SDL_IsGamepad SDL_IsGameController

// #define SDL_GAMEPAD_AXIS_INVALID SDL_CONTROLLER_AXIS_INVALID
#define SDL_GAMEPAD_AXIS_LEFTX SDL_CONTROLLER_AXIS_LEFTX
#define SDL_GAMEPAD_AXIS_LEFTY SDL_CONTROLLER_AXIS_LEFTY
// #define SDL_GAMEPAD_AXIS_COUNT SDL_CONTROLLER_AXIS_MAX
#define SDL_GAMEPAD_AXIS_RIGHTX SDL_CONTROLLER_AXIS_RIGHTX
#define SDL_GAMEPAD_AXIS_RIGHTY SDL_CONTROLLER_AXIS_RIGHTY
#define SDL_GAMEPAD_AXIS_LEFT_TRIGGER SDL_CONTROLLER_AXIS_TRIGGERLEFT
#define SDL_GAMEPAD_AXIS_RIGHT_TRIGGER SDL_CONTROLLER_AXIS_TRIGGERRIGHT
// #define SDL_GAMEPAD_BINDTYPE_AXIS SDL_CONTROLLER_BINDTYPE_AXIS
// #define SDL_GAMEPAD_BINDTYPE_BUTTON SDL_CONTROLLER_BINDTYPE_BUTTON
// #define SDL_GAMEPAD_BINDTYPE_HAT SDL_CONTROLLER_BINDTYPE_HAT
#define SDL_GAMEPAD_BINDTYPE_NONE SDL_CONTROLLER_BINDTYPE_NONE
#define SDL_GAMEPAD_BUTTON_SOUTH SDL_CONTROLLER_BUTTON_A
#define SDL_GAMEPAD_BUTTON_EAST SDL_CONTROLLER_BUTTON_B
// #define SDL_GAMEPAD_BUTTON_BACK SDL_CONTROLLER_BUTTON_BACK
#define SDL_GAMEPAD_BUTTON_DPAD_DOWN SDL_CONTROLLER_BUTTON_DPAD_DOWN
#define SDL_GAMEPAD_BUTTON_DPAD_LEFT SDL_CONTROLLER_BUTTON_DPAD_LEFT
#define SDL_GAMEPAD_BUTTON_DPAD_RIGHT SDL_CONTROLLER_BUTTON_DPAD_RIGHT
#define SDL_GAMEPAD_BUTTON_DPAD_UP SDL_CONTROLLER_BUTTON_DPAD_UP
// #define SDL_GAMEPAD_BUTTON_GUIDE SDL_CONTROLLER_BUTTON_GUIDE
// #define SDL_GAMEPAD_BUTTON_INVALID SDL_CONTROLLER_BUTTON_INVALID
// #define SDL_GAMEPAD_BUTTON_LEFT_SHOULDER SDL_CONTROLLER_BUTTON_LEFTSHOULDER
// #define SDL_GAMEPAD_BUTTON_LEFT_STICK SDL_CONTROLLER_BUTTON_LEFTSTICK
// #define SDL_GAMEPAD_BUTTON_COUNT SDL_CONTROLLER_BUTTON_MAX
// #define SDL_GAMEPAD_BUTTON_MISC1 SDL_CONTROLLER_BUTTON_MISC1
// #define SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1 SDL_CONTROLLER_BUTTON_PADDLE1
// #define SDL_GAMEPAD_BUTTON_LEFT_PADDLE1 SDL_CONTROLLER_BUTTON_PADDLE2
// #define SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2 SDL_CONTROLLER_BUTTON_PADDLE3
// #define SDL_GAMEPAD_BUTTON_LEFT_PADDLE2 SDL_CONTROLLER_BUTTON_PADDLE4
// #define SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
// #define SDL_GAMEPAD_BUTTON_RIGHT_STICK SDL_CONTROLLER_BUTTON_RIGHTSTICK
#define SDL_GAMEPAD_BUTTON_START SDL_CONTROLLER_BUTTON_START
// #define SDL_GAMEPAD_BUTTON_TOUCHPAD SDL_CONTROLLER_BUTTON_TOUCHPAD
// #define SDL_GAMEPAD_BUTTON_WEST SDL_CONTROLLER_BUTTON_X
// #define SDL_GAMEPAD_BUTTON_NORTH SDL_CONTROLLER_BUTTON_Y
// #define SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT
// #define SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR
// #define SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT
// #define SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO
// #define SDL_GAMEPAD_TYPE_PS3 SDL_CONTROLLER_TYPE_PS3
// #define SDL_GAMEPAD_TYPE_PS4 SDL_CONTROLLER_TYPE_PS4
// #define SDL_GAMEPAD_TYPE_PS5 SDL_CONTROLLER_TYPE_PS5
// #define SDL_GAMEPAD_TYPE_STANDARD SDL_CONTROLLER_TYPE_UNKNOWN
// #define SDL_GAMEPAD_TYPE_VIRTUAL SDL_CONTROLLER_TYPE_VIRTUAL
// #define SDL_GAMEPAD_TYPE_XBOX360 SDL_CONTROLLER_TYPE_XBOX360
// #define SDL_GAMEPAD_TYPE_XBOXONE SDL_CONTROLLER_TYPE_XBOXONE
