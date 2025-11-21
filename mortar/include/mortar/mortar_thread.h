#pragma once

#include "mortar_begin.h"

#include <stdint.h>

typedef struct MORTAR_Thread MORTAR_Thread;

typedef int (*MORTAR_ThreadFunction)(void* data);

extern MORTAR_DECLSPEC void MORTAR_WaitThread(MORTAR_Thread* thread, int* status);

extern MORTAR_DECLSPEC MORTAR_Thread* MORTAR_CreateThread(MORTAR_ThreadFunction fn, void* data, uint32_t stacksize);

#include "mortar_end.h"
