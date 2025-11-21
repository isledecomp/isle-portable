#pragma once

extern int MORTAR_SDL3_main(
	int argc,
	char* argv[],
	MORTAR_AppInit_cbfn* init,
	MORTAR_AppIterate_cbfn* iterate,
	MORTAR_AppEvent_cbfn* event,
	MORTAR_AppQuit_cbfn* quit
);

extern bool MORTAR_SDL3_Initialize(void);
