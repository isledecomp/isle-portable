#ifndef ISLEDEBUG_H
#define ISLEDEBUG_H

#if defined(ISLE_DEBUG)

extern bool g_debugEnabled;

typedef union SDL_Event SDL_Event;

extern void IsleDebug_Init();

extern bool IsleDebug_Event(SDL_Event* event);

extern void IsleDebug_Render();

#else

#define IsleDebug_Init()                                                                                               \
	do {                                                                                                               \
	} while (0)

#define IsleDebug_Event(EVENT) false

#define IsleDebug_Render()                                                                                             \
	do {                                                                                                               \
	} while (0)

#define g_debugEnabled false

#endif

#endif
