#pragma once

#include "mortar/mortar_joystick.h"
#include "mortar_begin.h"

#include <stdint.h>

typedef struct MORTAR_Haptic MORTAR_Haptic;

typedef uint32_t MORTAR_HapticID;

extern MORTAR_DECLSPEC bool MORTAR_InitHapticRumble(MORTAR_Haptic* haptic);

extern MORTAR_DECLSPEC MORTAR_HapticID* MORTAR_GetHaptics(int* count);

extern MORTAR_DECLSPEC MORTAR_Haptic* MORTAR_OpenHaptic(MORTAR_HapticID instance_id);

extern MORTAR_DECLSPEC void MORTAR_CloseHaptic(MORTAR_Haptic* haptic);

extern MORTAR_DECLSPEC MORTAR_Haptic* MORTAR_OpenHapticFromMouse(void);

extern MORTAR_DECLSPEC MORTAR_Haptic* MORTAR_OpenHapticFromJoystick(MORTAR_Joystick* joystick);

extern MORTAR_DECLSPEC bool MORTAR_PlayHapticRumble(MORTAR_Haptic* haptic, float strength, uint32_t length);

extern MORTAR_DECLSPEC MORTAR_HapticID MORTAR_GetHapticID(MORTAR_Haptic* haptic);

#include "mortar_end.h"
