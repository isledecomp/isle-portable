#pragma once

#include "mortar/mortar_video.h"
#include "mortar_begin.h"

#include <stdint.h>

typedef uint32_t MORTAR_MessageBoxFlags;

#define MORTAR_MESSAGEBOX_ERROR 0x00000010u
#define MORTAR_MESSAGEBOX_WARNING 0x00000020u
#define MORTAR_MESSAGEBOX_INFORMATION 0x00000040u
#define MORTAR_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT 0x00000080u
#define MORTAR_MESSAGEBOX_BUTTONS_RIGHT_TO_LEFT 0x00000100u

extern MORTAR_DECLSPEC bool MORTAR_ShowSimpleMessageBox(
	MORTAR_MessageBoxFlags flags,
	const char* title,
	const char* message,
	MORTAR_Window* window
);

#include "mortar_end.h"
