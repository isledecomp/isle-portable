#ifndef EMSCRIPTEN_HAPTIC_H
#define EMSCRIPTEN_HAPTIC_H

#include "mxtypes.h"

void Emscripten_HandleRumbleEvent(float p_lowFrequencyRumble, float p_highFrequencyRumble, MxU32 p_milliseconds);

#endif // EMSCRIPTEN_HAPTIC_H
