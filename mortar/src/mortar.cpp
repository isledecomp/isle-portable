#include "mortar/mortar.h"

#include "mortar/mortar_main.h"
#include "sdl3/sdl3_public.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static MORTAR_Backend mortar_backend = MORTAR_BACKEND_ANY;

// Define the default mortar backend here.
#define DEFAULT_MORTAR_BACKEND MORTAR_BACKEND_SDL3

MORTAR_Backend MORTAR_GetBackend()
{
	return mortar_backend;
}

bool MORTAR_SetBackend(MORTAR_Backend backend)
{
	if (mortar_backend != MORTAR_BACKEND_ANY && mortar_backend != backend) {
		return false;
	}
	mortar_backend = backend;
	return true;
}

bool MORTAR_ParseBackendName(const char* name, MORTAR_Backend* backend)
{
	if (strcmp(name, "sdl3") == 0 || strcmp(name, "SDL3") == 0) {
		*backend = MORTAR_BACKEND_SDL3;
		return true;
	}
	return false;
}

static bool parse_mortar_arguments(int argc, char* argv[])
{
	for (int i = 1; i < argc;) {
		int consumed = 1;
		if (strcmp(argv[i], "--platform") == 0) {
			if (i + 1 >= argc || argv[i + 1][0] == '-') {
				fprintf(stderr, "--platform requires an argument\n");
				return false;
			}
			if (mortar_backend != MORTAR_BACKEND_ANY) {
				fprintf(stderr, "--platform can only be used once\n");
				return false;
			}
			if (MORTAR_ParseBackendName(argv[i + 1], &mortar_backend)) {
				consumed = 2;
			}
			else {
				fprintf(stderr, "Invalid mortar platform: %s\n", argv[i + 1]);
				return false;
			}
		}
		i += consumed;
	}
	return true;
}

bool MORTAR_InitializeBackend(void)
{
	if (mortar_backend == MORTAR_BACKEND_ANY) {
		mortar_backend = MORTAR_BACKEND_SDL3;
	}
	switch (mortar_backend) {
	case MORTAR_BACKEND_SDL3:
		return MORTAR_SDL3_Initialize();
	default:
		fprintf(stderr, "** INTERNAL ERROR: Unknown platform **\n");
		return false;
	}
}

int MORTAR_main(
	int argc,
	char* argv[],
	MORTAR_AppInit_cbfn* init,
	MORTAR_AppIterate_cbfn* iterate,
	MORTAR_AppEvent_cbfn* event,
	MORTAR_AppQuit_cbfn* quit
)
{
	if (!parse_mortar_arguments(argc, argv)) {
		return 1;
	}
	if (!MORTAR_InitializeBackend()) {
		return 1;
	}
	switch (mortar_backend) {
	case MORTAR_BACKEND_SDL3:
		return MORTAR_SDL3_main(argc, argv, init, iterate, event, quit);
	default:
		fprintf(stderr, "** INTERNAL ERROR: Unknown platform **\n");
		return 1;
	}
}
