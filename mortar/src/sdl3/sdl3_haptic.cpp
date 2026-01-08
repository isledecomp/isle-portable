#include "mortar/mortar_haptic.h"
#include "sdl3_internal.h"

SDL_Haptic* haptic_mortar_to_sdl3(MORTAR_Haptic* haptic)
{
	return (SDL_Haptic*) haptic;
}

MORTAR_Haptic* haptic_sdl3_to_mortar(SDL_Haptic* haptic)
{
	return (MORTAR_Haptic*) haptic;
}

bool MORTAR_InitHapticRumble(MORTAR_Haptic* haptic)
{
	return SDL_InitHapticRumble(haptic_mortar_to_sdl3(haptic));
}

MORTAR_HapticID* MORTAR_GetHaptics(int* count)
{
	return (MORTAR_HapticID*) SDL_GetHaptics(count);
}

MORTAR_Haptic* MORTAR_OpenHaptic(MORTAR_HapticID instance_id)
{
	return haptic_sdl3_to_mortar(SDL_OpenHaptic(instance_id));
}

void MORTAR_CloseHaptic(MORTAR_Haptic* haptic)
{
	SDL_CloseHaptic(haptic_mortar_to_sdl3(haptic));
}

MORTAR_Haptic* MORTAR_OpenHapticFromMouse()
{
	return haptic_sdl3_to_mortar(SDL_OpenHapticFromMouse());
}

MORTAR_Haptic* MORTAR_OpenHapticFromJoystick(MORTAR_Joystick* joystick)
{
	return haptic_sdl3_to_mortar(SDL_OpenHapticFromJoystick(joystick_mortar_to_sdl3(joystick)));
}

bool MORTAR_PlayHapticRumble(MORTAR_Haptic* haptic, float strength, uint32_t length)
{
	return SDL_PlayHapticRumble(haptic_mortar_to_sdl3(haptic), strength, length);
}

MORTAR_HapticID MORTAR_GetHapticID(MORTAR_Haptic* haptic)
{
	return (MORTAR_HapticID) SDL_GetHapticID(haptic_mortar_to_sdl3(haptic));
}
