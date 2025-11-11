#ifndef WIIU_APTHOOKS_H
#define WIIU_APTHOOKS_H

#include <wut.h>

void WIIU_SetupAptHooks();
void WIIU_ProcessCallbacks();
bool WIIU_AppIsRunning();

#endif
