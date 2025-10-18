#pragma once

// https://wiki.libsdl.org/SDL3/README-migration#sdl_stdinch
#define SDL_bool bool

#include <SDL2/SDL.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_mouse.h>

#include "SDL_audio.h"
#include "SDL_events.h"
#include "SDL_filesystem.h"
#include "SDL_gamepad.h"
#include "SDL_iostream.h"
#include "SDL_keycode.h"
#include "SDL_main.h"
#include "SDL_mutex.h"
#include "SDL_pixels.h"
#include "SDL_surface.h"
#include "SDL_timer.h"

#include <random>

static std::mt19937 rng(SDL_GetPerformanceCounter());
inline Sint32 SDL_rand(const Sint32 n)
{
	std::uniform_int_distribution dist(0, n - 1);
	return dist(rng);
}
inline float SDL_randf()
{
	static std::uniform_real_distribution dist(0.0f, 1.0f);
	return dist(rng);
}

#define SDL_strtok_r SDL_strtokr

// https://wiki.libsdl.org/SDL3/README-migration#sdl_logh

#define SDL_LogTrace SDL_LogVerbose

// https://wiki.libsdl.org/SDL3/README-migration#sdl_videoh

typedef Uint32 SDL_DisplayID;
inline SDL_DisplayID SDL_GetPrimaryDisplay()
{
	return 0;
}

// Modified from 83bb0f9105922fd49282f0b931f7873a71877ac8 SDL_video.c#L1331
inline SDL_DisplayMode** SDL_GetFullscreenDisplayModes(SDL_DisplayID displayID, int *count)
{
	int i;
	if (count) *count = 0;

	const int num_modes = SDL_GetNumDisplayModes(displayID);
    SDL_DisplayMode** result = static_cast<SDL_DisplayMode**>(SDL_malloc(sizeof(SDL_DisplayMode*) * (num_modes + 1)));
	SDL_DisplayMode* modes = static_cast<SDL_DisplayMode*>(SDL_malloc(sizeof(SDL_DisplayMode) * num_modes));
	if (!result || !modes) {
		SDL_free(result);
		SDL_free(modes);
		return NULL;
	}
	for (i = 0; i < num_modes; i++) {
		if (SDL_GetDisplayMode(displayID, i, &modes[i]) == 0) {
			result[i] = &modes[i];
		}
    }
	result[i] = NULL;

	if (count) {
		*count = num_modes;
	}

    return result;
}

inline SDL_DisplayMode* SDL_GetCurrentDisplayMode(SDL_DisplayID displayID)
{
	SDL_DisplayMode* mode = nullptr;
	SDL_GetCurrentDisplayMode(displayID, mode);
	return mode;
}

#define SDL_GetWindowSize(...) (SDL_GetWindowSize(__VA_ARGS__), true )

// https://wiki.libsdl.org/SDL3/README-migration#sdl_videoh

#define SDL_GL_DestroyContext SDL_GL_DeleteContext

// https://wiki.libsdl.org/SDL3/README-migration#sdl_renderh

// hardcode -1 as all uses are NULL or -1 (hacks out failure)
#define SDL_CreateRenderer(window, name) SDL_CreateRenderer(window, -1, 0)

#define SDL_RenderTexture(...) (SDL_RenderCopyF(__VA_ARGS__) == 0)

// https://wiki.libsdl.org/SDL3/README-migration#sdl_haptich
// SDL_MouseID/SDL_KeyboardID are new

typedef int SDL_KeyboardID;
#define SDL_GetKeyboardState (const bool*)SDL_GetKeyboardState
typedef int SDL_HapticID;


#define SDL_CloseHaptic SDL_HapticClose
#define SDL_OpenHaptic SDL_HapticOpen
#define SDL_OpenHapticFromJoystick SDL_HapticOpenFromJoystick
#define SDL_OpenHapticFromMouse SDL_HapticOpenFromMouse
#define SDL_InitHapticRumble(...) (SDL_HapticRumbleInit(__VA_ARGS__) == 0)
#define SDL_PlayHapticRumble(...) (SDL_HapticRumblePlay(__VA_ARGS__) == 0)

#define SDL_GetHapticID SDL_HapticIndex

// Modified from cc9937201e421ec55b12ad3f07ff2268f15096e8 SDL_haptic.c#L150
inline SDL_HapticID* SDL_GetHaptics(int *count)
{
	const int num_haptics = SDL_NumHaptics();
	if (count) *count = 0;

	SDL_HapticID* haptics = static_cast<SDL_HapticID*>(SDL_malloc((num_haptics + 1) * sizeof(*haptics)));
	if (haptics) {
		if (count) {
			*count = num_haptics;
		}
		int haptic_index = 0;
		for (int device_index = 0; device_index < num_haptics; ++device_index) {
			SDL_Haptic* haptic = SDL_HapticOpen(device_index);
			if (haptic) {
				haptics[haptic_index] = SDL_GetHapticID(haptic);
				SDL_HapticClose(haptic);
				++haptic_index;
			}
		}
		haptics[haptic_index] = 0;
	} else { if (count) *count = 0; }

	return haptics;
}

// https://wiki.libsdl.org/SDL3/README-migration#sdl_videoh

#define SDL_CreateWindow(title, w, h, flags) SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, flags)
#define SDL_SetWindowFullscreen(...) (SDL_SetWindowFullscreen(__VA_ARGS__) == 0)

#define SDL_GL_MakeCurrent(...) (SDL_GL_MakeCurrent(__VA_ARGS__) == 0)

#define SDL_GetDisplayForWindow SDL_GetWindowDisplayIndex
#define SDL_SetWindowFullscreenMode(...) (SDL_SetWindowDisplayMode(__VA_ARGS__) == 0)
// #define SDL_GetClosestFullscreenDisplayMode SDL_GetClosestDisplayMode
inline bool SDL_GetClosestFullscreenDisplayMode(SDL_DisplayID displayID, int w, int h, float refresh_rate, bool include_high_density_modes, SDL_DisplayMode *closest)
{
	SDL_DisplayMode desired{};
	desired.w = w;
	desired.h = h;
	desired.refresh_rate = static_cast<int>(refresh_rate);
	desired.format = 0;
	desired.driverdata = nullptr;

	SDL_DisplayMode *result = SDL_GetClosestDisplayMode(displayID, &desired, closest);
	return result != nullptr;
}


// https://wiki.libsdl.org/SDL3/README-migration#sdl_mouseh

typedef Uint32 SDL_MouseID;

inline SDL_MouseID * SDL_GetMice(int *count)
{
	if (count) {
		*count = 1;
	}
	const auto mice = static_cast<SDL_MouseID *>(SDL_malloc((*count + 1) * sizeof(SDL_MouseID)));
	mice[0] = { 0 };
	mice[1] = { 0 };
	return mice;
}

static void SDL_HideCursor() { SDL_ShowCursor(SDL_DISABLE); }
static void SDL_ShowCursor() { SDL_ShowCursor(SDL_ENABLE); }

typedef Uint32 SDL_MouseButtonFlags;

#define SDL_SYSTEM_CURSOR_COUNT SDL_NUM_SYSTEM_CURSORS
#define SDL_SYSTEM_CURSOR_DEFAULT SDL_SYSTEM_CURSOR_ARROW
#define SDL_SYSTEM_CURSOR_POINTER SDL_SYSTEM_CURSOR_HAND
#define SDL_SYSTEM_CURSOR_TEXT SDL_SYSTEM_CURSOR_IBEAM
#define SDL_SYSTEM_CURSOR_NOT_ALLOWED SDL_SYSTEM_CURSOR_NO
#define SDL_SYSTEM_CURSOR_MOVE SDL_SYSTEM_CURSOR_SIZEALL
#define SDL_SYSTEM_CURSOR_NESW_RESIZE SDL_SYSTEM_CURSOR_SIZENESW
#define SDL_SYSTEM_CURSOR_NS_RESIZE SDL_SYSTEM_CURSOR_SIZENS
#define SDL_SYSTEM_CURSOR_NWSE_RESIZE SDL_SYSTEM_CURSOR_SIZENWSE
#define SDL_SYSTEM_CURSOR_EW_RESIZE SDL_SYSTEM_CURSOR_SIZEWE
#define SDL_SYSTEM_CURSOR_PROGRESS SDL_SYSTEM_CURSOR_WAITARROW

// https://wiki.libsdl.org/SDL3/README-migration#sdl_inith

#define SDL_Init(...) (SDL_Init(__VA_ARGS__) == 0)
