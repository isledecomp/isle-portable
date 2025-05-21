#pragma once

// --- Defines and Macros ---
#define _P_NOWAIT 1

// --- Functions ---
intptr_t _spawnl(
	int mode,
	const char* cmdname,
	const char* arg0,
	const char* arg1,
	const char* arg2,
	const char* arg3,
	...
);
