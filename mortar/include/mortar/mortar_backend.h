#pragma once

#include "mortar_begin.h"

typedef enum {
	MORTAR_BACKEND_SDL3,
	MORTAR_BACKEND_ANY
} MORTAR_Backend;

extern MORTAR_DECLSPEC bool MORTAR_ParseBackendName(const char* name, MORTAR_Backend* backend);

extern MORTAR_DECLSPEC bool MORTAR_SetBackend(MORTAR_Backend backend);

extern MORTAR_DECLSPEC MORTAR_Backend MORTAR_GetBackend(void);

extern MORTAR_DECLSPEC const char* MORTAR_GetBackendDescription(void);

extern MORTAR_DECLSPEC bool MORTAR_InitializeBackend(void);

#include "mortar_end.h"
