#ifndef EMSCRIPTEN_EVENTS_H
#define EMSCRIPTEN_EVENTS_H

#include "mxpresenter.h"

void Emscripten_SendPresenterProgress(MxPresenter* p_presenter, MxPresenter::TickleState p_tickleState);

#endif // EMSCRIPTEN_EVENTS_H
