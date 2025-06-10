#ifndef ISLEDEBUG_H
#define ISLEDEBUG_H

#if defined(ISLE_DEBUG)

typedef union SDL_Event SDL_Event;

extern bool IsleDebug_Enabled();

extern void IsleDebug_SetEnabled(bool);

extern void IsleDebug_Init();

extern void IsleDebug_Quit();

extern bool IsleDebug_Event(SDL_Event* event);

extern void IsleDebug_Render();

extern void IsleDebug_SetPaused(bool v);

extern bool IsleDebug_Paused();

extern bool IsleDebug_StepModeEnabled();

extern void IsleDebug_ResetStepMode();

#else

#define IsleDebug_Enabled() (false)

#define IsleDebug_SetEnabled(V)                                                                                        \
	do {                                                                                                               \
	} while (0)

#define IsleDebug_Init()                                                                                               \
	do {                                                                                                               \
	} while (0)

#define IsleDebug_Quit()                                                                                               \
	do {                                                                                                               \
	} while (0)

#define IsleDebug_Event(EVENT) (false)

#define IsleDebug_Render()                                                                                             \
	do {                                                                                                               \
	} while (0)

#define IsleDebug_SetPaused(X)                                                                                         \
	do {                                                                                                               \
	} while (0)

#define IsleDebug_Paused() (false)

#define IsleDebug_StepModeEnabled() (false)

#define IsleDebug_ResetStepMode()                                                                                      \
	do {                                                                                                               \
	} while (0)

#endif

#endif
