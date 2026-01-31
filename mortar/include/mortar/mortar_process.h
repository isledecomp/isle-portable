#pragma once

#include "mortar_begin.h"

typedef struct MORTAR_Process MORTAR_Process;

extern MORTAR_DECLSPEC MORTAR_Process* MORTAR_CreateProcess(const char* const* args, bool pipe_stdio);

#include "mortar_end.h"
