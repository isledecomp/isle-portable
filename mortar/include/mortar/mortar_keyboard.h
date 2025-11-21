#pragma once

#include "mortar_begin.h"

#include <stdint.h>

typedef uint32_t MORTAR_KeyboardID;

extern MORTAR_DECLSPEC const bool* MORTAR_GetKeyboardState(int* numkeys);

#include "mortar_end.h"
