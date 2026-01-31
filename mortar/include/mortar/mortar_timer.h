#pragma once

#include "mortar_begin.h"

#include <stdint.h>

typedef uint32_t MORTAR_TimerID;

typedef uint32_t (*MORTAR_TimerCallback)(void* userdata, MORTAR_TimerID timerID, uint32_t interval);

#define MORTAR_NS_PER_MS 1000000

#define MORTAR_NS_TO_MS(NS) ((NS) / MORTAR_NS_PER_MS)

extern MORTAR_DECLSPEC void MORTAR_Delay(uint32_t ms);

extern MORTAR_DECLSPEC uint64_t MORTAR_GetTicks(void);

extern MORTAR_DECLSPEC uint64_t MORTAR_GetTicksNS(void);

extern MORTAR_DECLSPEC uint64_t MORTAR_GetPerformanceFrequency(void);

extern MORTAR_DECLSPEC uint64_t MORTAR_GetPerformanceCounter(void);

extern MORTAR_DECLSPEC MORTAR_TimerID MORTAR_AddTimer(uint32_t interval, MORTAR_TimerCallback callback, void* userdata);

extern MORTAR_DECLSPEC bool MORTAR_RemoveTimer(MORTAR_TimerID id);

#include "mortar_end.h"
