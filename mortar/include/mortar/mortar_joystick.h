#pragma once

#include <stdint.h>

#include "mortar_begin.h"

#define MORTAR_JOYSTICK_AXIS_MAX   32767

typedef struct MORTAR_Joystick MORTAR_Joystick;

typedef uint32_t MORTAR_JoystickID;

extern MORTAR_DECLSPEC const char * MORTAR_GetJoystickNameForID(MORTAR_JoystickID instance_id);

#include "mortar_end.h"

