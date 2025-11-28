#pragma once

#include "mortar_begin.h"

typedef enum {
	MORTAR_BACKEND_SDL3
} MORTAR_Backend;

extern MORTAR_DECLSPEC  MORTAR_Backend MORTAR_GetBackend(void);

#include "mortar_end.h"
