#include "mortar/mortar.h"
#include "mortar/mortar_main.h"

#include "sdl3/sdl3_public.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static MORTAR_Backend mortar_backend = MORTAR_BACKEND_SDL3;

MORTAR_Backend MORTAR_GetBackend()
{
	return mortar_backend;
}

int MORTAR_main(int argc, char *argv[], MORTAR_AppInit_cbfn *init, MORTAR_AppIterate_cbfn *iterate, MORTAR_AppEvent_cbfn *event, MORTAR_AppQuit_cbfn *quit)
{
	for (int i = 1; i < argc;) {
		int consumed = 1;
		if (strcmp(argv[i], "--platform") == 0) {
			if (i + 1 >= argc || argv[i + 1][0] == '-') {
				fprintf(stderr, "--platform requires an argument\n");
				return 1;
			}
			const char *req_mortar_backend_str = argv[i + 1];
			if (strcmp(req_mortar_backend_str, "sdl3") == 0 || strcmp(req_mortar_backend_str, "SDL3") == 0) {
				mortar_backend = MORTAR_BACKEND_SDL3;
				consumed = 2;
			} else {
				fprintf(stderr, "Unknown platform (%s)\n", req_mortar_backend_str);
				return 1;
			}
		}
		i += consumed;
	}
	/* FIXME: parse arguments for --platform sdl3 */
	switch (mortar_backend) {
	case MORTAR_BACKEND_SDL3:
		return MORTAR_SDL3_main(argc, argv, init, iterate, event, quit);
	default:
		fprintf(stderr, "** INTERNAL ERROR: Unknown platform **\n");
		return 1;
	}
}
