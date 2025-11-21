#pragma once

#include "mortar_begin.h"

extern MORTAR_DECLSPEC bool MORTAR_HasMMX(void);

extern MORTAR_DECLSPEC bool MORTAR_HasSSE2(void);

extern MORTAR_DECLSPEC bool MORTAR_HasNEON(void);

extern MORTAR_DECLSPEC int MORTAR_GetSystemRAM(void);

#include "mortar_end.h"
