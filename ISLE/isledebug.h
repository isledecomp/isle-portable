#ifndef ISLEDEBUG_H
#define ISLEDEBUG_H

extern bool g_debugEnabled;

typedef union SDL_Event SDL_Event;

extern void IsleDebug_Init();

extern bool IsleDebug_Event(SDL_Event* event);

extern void IsleDebug_Render();

#endif
