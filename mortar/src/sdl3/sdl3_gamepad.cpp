#include "mortar/mortar_gamepad.h"
#include "sdl3_internal.h"

#include <stdlib.h>

SDL_Gamepad* gamepad_mortar_to_sdl3(MORTAR_Gamepad* gamepad)
{
	return (SDL_Gamepad*) gamepad;
}

SDL_GamepadAxis gamepadax_mortar_to_sdl3(MORTAR_GamepadAxis axis)
{
	switch (axis) {
	case MORTAR_GAMEPAD_AXIS_LEFTX:
		return SDL_GAMEPAD_AXIS_LEFTX;
	case MORTAR_GAMEPAD_AXIS_LEFTY:
		return SDL_GAMEPAD_AXIS_LEFTY;
	case MORTAR_GAMEPAD_AXIS_RIGHTX:
		return SDL_GAMEPAD_AXIS_RIGHTX;
	case MORTAR_GAMEPAD_AXIS_RIGHTY:
		return SDL_GAMEPAD_AXIS_RIGHTY;
	case MORTAR_GAMEPAD_AXIS_RIGHT_TRIGGER:
		return SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
	default:
		abort();
	}
}

MORTAR_Gamepad* MORTAR_OpenGamepad(MORTAR_JoystickID instance_id)
{
	return (MORTAR_Gamepad*) SDL_OpenGamepad(instance_id);
}

void MORTAR_CloseGamepad(MORTAR_Gamepad* gamepad)
{
	SDL_CloseGamepad(gamepad_mortar_to_sdl3(gamepad));
}

int16_t MORTAR_GetGamepadAxis(MORTAR_Gamepad* gamepad, MORTAR_GamepadAxis axis)
{
	return SDL_GetGamepadAxis(gamepad_mortar_to_sdl3(gamepad), gamepadax_mortar_to_sdl3(axis));
}

MORTAR_Joystick* MORTAR_GetGamepadJoystick(MORTAR_Gamepad* gamepad)
{
	return (MORTAR_Joystick*) SDL_GetGamepadJoystick(gamepad_mortar_to_sdl3(gamepad));
}

bool MORTAR_RumbleGamepad(
	MORTAR_Gamepad* gamepad,
	uint16_t low_frequency_rumble,
	uint16_t high_frequency_rumble,
	uint32_t duration_ms
)
{
	return SDL_RumbleGamepad(gamepad_mortar_to_sdl3(gamepad), low_frequency_rumble, high_frequency_rumble, duration_ms);
}
