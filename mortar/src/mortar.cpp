#include "mortar/mortar.h"
#include "mortar/mortar_main.h"

#include "sdl3/sdl3_public.h"

#include <stdlib.h>
#include <string.h>

MORTAR_Backend MORTAR_GetBackend()
{
	return MORTAR_BACKEND_SDL3;
}

int MORTAR_main(int argc, char *argv[], MORTAR_AppInit_cbfn *init, MORTAR_AppIterate_cbfn *iterate, MORTAR_AppEvent_cbfn *event, MORTAR_AppQuit_cbfn *quit)
{
	/* FIXME: parse arguments for --platform sdl3 */
	return MORTAR_SDL3_main(argc, argv, init, iterate, event, quit);
}
