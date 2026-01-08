#include "mortar/mortar_joystick.h"
#include "sdl3_internal.h"

SDL_Joystick* joystick_mortar_to_sdl3(MORTAR_Joystick* joystick)
{
	return (SDL_Joystick*) joystick;
}

MORTAR_DECLSPEC const char* MORTAR_GetJoystickNameForID(MORTAR_JoystickID instance_id)
{
	return SDL_GetJoystickNameForID((SDL_JoystickID) instance_id);
}
