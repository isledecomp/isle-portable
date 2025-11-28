#pragma once

#include <stdint.h>

#include "mortar_begin.h"

typedef uint32_t MORTAR_KeyboardID;

extern MORTAR_DECLSPEC const bool * MORTAR_GetKeyboardState(int *numkeys);

#include "mortar_end.h"
