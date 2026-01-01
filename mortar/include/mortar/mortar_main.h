#pragma once
#include "mortar/mortar_events.h"
#include "mortar_begin.h"

typedef enum MORTAR_AppResult {
	MORTAR_APP_CONTINUE,
	MORTAR_APP_SUCCESS,
	MORTAR_APP_FAILURE
} MORTAR_AppResult;

typedef MORTAR_AppResult MORTAR_AppInit_cbfn(void** appstate, int argc, char* argv[]);
typedef MORTAR_AppResult MORTAR_AppIterate_cbfn(void* appstate);
typedef MORTAR_AppResult MORTAR_AppEvent_cbfn(void* appstate, MORTAR_Event* event);
typedef void MORTAR_AppQuit_cbfn(void* appstate, MORTAR_AppResult result);

extern MORTAR_DECLSPEC int MORTAR_main(
	int argc,
	char* argv[],
	MORTAR_AppInit_cbfn* init,
	MORTAR_AppIterate_cbfn* iterate,
	MORTAR_AppEvent_cbfn* event,
	MORTAR_AppQuit_cbfn* quit
);

#include "mortar_end.h"
