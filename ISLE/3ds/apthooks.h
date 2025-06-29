#ifndef N3DS_APTHOOKS_H
#define N3DS_APTHOOKS_H

#include <3ds.h>

void N3DS_AptHookCallback(APT_HookType hookType, void* param);
void N3DS_SetupAptHooks();

#endif // N3DS_APTHOOKS_H
