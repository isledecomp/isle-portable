#ifndef EMSCRIPTEN_EVENTS_H
#define EMSCRIPTEN_EVENTS_H

#include "mxpresenter.h"

void Emscripten_SendPresenterProgress(MxDSAction* p_action, MxPresenter::TickleState p_tickleState);
void Emscripten_SendExtensionProgress(const char* p_extension, MxU32 p_progress);
void Emscripten_SendSaveSlotWritten(MxS32 p_slot);
void Emscripten_SendSaveStateChanged();

#endif // EMSCRIPTEN_EVENTS_H
