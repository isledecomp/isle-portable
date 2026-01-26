#pragma once

#include "mortar_begin.h"

#include <stdint.h>

typedef struct MORTAR_Mutex MORTAR_Mutex;
typedef struct MORTAR_Semaphore MORTAR_Semaphore;

extern MORTAR_DECLSPEC MORTAR_Mutex* MORTAR_CreateMutex(void);

extern MORTAR_DECLSPEC void MORTAR_LockMutex(MORTAR_Mutex* mutex);

extern MORTAR_DECLSPEC void MORTAR_UnlockMutex(MORTAR_Mutex* mutex);

extern MORTAR_DECLSPEC void MORTAR_DestroyMutex(MORTAR_Mutex* mutex);

extern MORTAR_DECLSPEC MORTAR_Semaphore* MORTAR_CreateSemaphore(uint32_t initial_value);

extern MORTAR_DECLSPEC void MORTAR_SignalSemaphore(MORTAR_Semaphore* sem);

extern MORTAR_DECLSPEC void MORTAR_WaitSemaphore(MORTAR_Semaphore* sem);

extern MORTAR_DECLSPEC void MORTAR_DestroySemaphore(MORTAR_Semaphore* sem);

#include "mortar_end.h"
