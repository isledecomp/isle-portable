#include "mortar/mortar_video.h"
#include "sdl3_internal.h"

#include <stdlib.h>
#include <string>
#include <unordered_map>

typedef struct MORTAR_Window {
	SDL_Window* sdl3_window = nullptr;
	void* user_property = nullptr;
} MORTAR_Window;

SDL_Window* window_mortar_to_sdl3(MORTAR_Window* mortar)
{
	if (!mortar) {
		return nullptr;
	}
	return mortar->sdl3_window;
}

const char* MORTAR_GetCurrentVideoDriver()
{
	return SDL_GetCurrentVideoDriver();
}

bool MORTAR_RaiseWindow(MORTAR_Window* mortar_window)
{
	return SDL_RaiseWindow(window_mortar_to_sdl3(mortar_window));
}

bool MORTAR_SetWindowSize(MORTAR_Window* mortar_window, int w, int h)
{
	return SDL_SetWindowSize(window_mortar_to_sdl3(mortar_window), w, h);
}

bool MORTAR_SetWindowPosition(MORTAR_Window* mortar_window, int x, int y)
{
	return SDL_SetWindowPosition(window_mortar_to_sdl3(mortar_window), x, y);
}

MORTAR_WindowFlags MORTAR_GetWindowFlags(MORTAR_Window* mortar_window)
{
	SDL_WindowFlags sdl3_flags = SDL_GetWindowFlags(window_mortar_to_sdl3(mortar_window));
	MORTAR_WindowFlags mortar_flags = (MORTAR_WindowFlags) 0;
	if (sdl3_flags & SDL_WINDOW_FULLSCREEN) {
		mortar_flags |= MORTAR_WINDOW_FULLSCREEN;
	}
	if (sdl3_flags & SDL_WINDOW_BORDERLESS) {
		mortar_flags |= MORTAR_WINDOW_BORDERLESS;
	}
	if (sdl3_flags & SDL_WINDOW_RESIZABLE) {
		mortar_flags |= MORTAR_WINDOW_RESIZABLE;
	}
	return mortar_flags;
}

bool MORTAR_SetWindowBordered(MORTAR_Window* mortar_window, bool bordered)
{
	return SDL_SetWindowBordered(window_mortar_to_sdl3(mortar_window), bordered);
}

bool MORTAR_SetWindowResizable(MORTAR_Window* mortar_window, bool resizable)
{
	return SDL_SetWindowResizable(window_mortar_to_sdl3(mortar_window), resizable);
}

bool MORTAR_GetWindowSize(MORTAR_Window* mortar_window, int* w, int* h)
{
	return SDL_GetWindowSize(window_mortar_to_sdl3(mortar_window), w, h);
}

bool MORTAR_GetWindowSizeInPixels(MORTAR_Window* mortar_window, int* w, int* h)
{
	return SDL_GetWindowSizeInPixels(window_mortar_to_sdl3(mortar_window), w, h);
}

MORTAR_DisplayID MORTAR_GetPrimaryDisplay()
{
	return SDL_GetPrimaryDisplay();
}

MORTAR_DisplayMode** MORTAR_GetFullscreenDisplayModes(MORTAR_DisplayID displayID, int* count)
{
	int sdl3_count = 0;
	SDL_DisplayMode** sdl3_modes = SDL_GetFullscreenDisplayModes(displayID, &sdl3_count);
	if (!sdl3_modes) {
		if (count) {
			*count = 0;
		}
		return nullptr;
	}
	uint8_t* buffer =
		(uint8_t*) SDL_malloc(sizeof(MORTAR_DisplayMode*) * (sdl3_count + 1) + sizeof(MORTAR_DisplayMode) * sdl3_count);
	MORTAR_DisplayMode** mortar_modes_pointers = (MORTAR_DisplayMode**) buffer;
	MORTAR_DisplayMode* mortar_modes_data =
		(MORTAR_DisplayMode*) (buffer + sizeof(MORTAR_DisplayMode*) * (sdl3_count + 1));
	for (int i = 0; i < sdl3_count; i++) {
		mortar_modes_pointers[i] = &mortar_modes_data[i];
		mortar_modes_data[i].format = pixelformat_sdl3_to_mortar(sdl3_modes[i]->format);
		mortar_modes_data[i].w = sdl3_modes[i]->w;
		mortar_modes_data[i].h = sdl3_modes[i]->h;
		mortar_modes_data[i].refresh_rate = sdl3_modes[i]->refresh_rate;
	}
	mortar_modes_pointers[sdl3_count] = nullptr;
	if (count) {
		*count = sdl3_count;
	}
	SDL_free(sdl3_modes);
	return mortar_modes_pointers;
}

const MORTAR_DisplayMode* MORTAR_GetCurrentDisplayMode(MORTAR_DisplayID displayID)
{
	static MORTAR_DisplayMode mortar_display_mode;
	const SDL_DisplayMode* sdl3_mode = SDL_GetCurrentDisplayMode((SDL_DisplayID) displayID);
	if (!sdl3_mode) {
		return nullptr;
	}
	mortar_display_mode.format = pixelformat_sdl3_to_mortar(sdl3_mode->format);
	mortar_display_mode.w = sdl3_mode->w;
	mortar_display_mode.h = sdl3_mode->h;
	mortar_display_mode.refresh_rate = sdl3_mode->refresh_rate;
	return &mortar_display_mode;
}

bool MORTAR_SetWindowFullscreenMode(MORTAR_Window* mortar_window, const MORTAR_DisplayMode* mode)
{
	if (!mode) {
		return SDL_SetError("mode");
	}
	SDL_DisplayMode sdl3_mode;
	SDL_zero(sdl3_mode);
	sdl3_mode.w = mode->w;
	sdl3_mode.h = mode->h;
	sdl3_mode.format = pixelformat_mortar_to_sdl3(mode->format);
	return SDL_SetWindowFullscreenMode(window_mortar_to_sdl3(mortar_window), &sdl3_mode);
}

bool MORTAR_SetWindowFullscreen(MORTAR_Window* mortar_window, bool fullscreen)
{
	return SDL_SetWindowFullscreen(window_mortar_to_sdl3(mortar_window), fullscreen);
}

bool MORTAR_GL_ExtensionSupported(const char* extension)
{
	return SDL_GL_ExtensionSupported(extension);
}

MORTAR_FunctionPointer MORTAR_GL_GetProcAddress(const char* proc)
{
	return SDL_GL_GetProcAddress(proc);
}

void MORTAR_GL_ResetAttributes()
{
	SDL_GL_ResetAttributes();
}

bool MORTAR_GL_SetAttribute(MORTAR_GLAttr attr, int value)
{
	SDL_GLAttr sdl3_attr;
	switch (attr) {
	case MORTAR_GL_DOUBLEBUFFER:
		sdl3_attr = SDL_GL_DOUBLEBUFFER;
		break;
	case MORTAR_GL_DEPTH_SIZE:
		sdl3_attr = SDL_GL_DEPTH_SIZE;
		break;
	case MORTAR_GL_MULTISAMPLEBUFFERS:
		sdl3_attr = SDL_GL_MULTISAMPLEBUFFERS;
		break;
	case MORTAR_GL_MULTISAMPLESAMPLES:
		sdl3_attr = SDL_GL_MULTISAMPLESAMPLES;
		break;
	case MORTAR_GL_CONTEXT_MAJOR_VERSION:
		sdl3_attr = SDL_GL_CONTEXT_MAJOR_VERSION;
		break;
	case MORTAR_GL_CONTEXT_MINOR_VERSION:
		sdl3_attr = SDL_GL_CONTEXT_MINOR_VERSION;
		break;
	case MORTAR_GL_CONTEXT_PROFILE_MASK:
		sdl3_attr = SDL_GL_CONTEXT_PROFILE_MASK;
		break;
	default:
		abort();
	}
	return SDL_GL_SetAttribute(sdl3_attr, value);
}

bool MORTAR_GL_MakeCurrent(MORTAR_Window* mortar_window, MORTAR_GLContext context)
{
	return SDL_GL_MakeCurrent(window_mortar_to_sdl3(mortar_window), (SDL_GLContext) context);
}

MORTAR_GLContext MORTAR_GL_CreateContext(MORTAR_Window* mortar_window)
{
	return (MORTAR_GLContext) SDL_GL_CreateContext(window_mortar_to_sdl3(mortar_window));
}

bool MORTAR_GL_DestroyContext(MORTAR_GLContext context)
{
	return SDL_GL_DestroyContext((SDL_GLContext) context);
}

bool MORTAR_GL_SwapWindow(MORTAR_Window* mortar_window)
{
	return SDL_GL_SwapWindow(window_mortar_to_sdl3(mortar_window));
}

bool MORTAR_SetWindowTitle(MORTAR_Window* mortar_window, const char* title)
{
	return SDL_SetWindowTitle(window_mortar_to_sdl3(mortar_window), title);
}

bool MORTAR_SetWindowIcon(MORTAR_Window* mortar_window, MORTAR_Surface* icon)
{
	return SDL_SetWindowIcon(window_mortar_to_sdl3(mortar_window), surface_mortar_to_sdl3(icon));
}

bool MORTAR_GetClosestFullscreenDisplayMode(
	MORTAR_DisplayID displayID,
	int w,
	int h,
	float refresh_rate,
	bool include_high_density_modes,
	MORTAR_DisplayMode* closest
)
{
	SDL_DisplayMode closest_sdl3;
	if (!SDL_GetClosestFullscreenDisplayMode(
			(SDL_DisplayID) displayID,
			w,
			h,
			refresh_rate,
			include_high_density_modes,
			&closest_sdl3
		)) {
		return false;
	}
	closest->w = closest_sdl3.w;
	closest->h = closest_sdl3.h;
	closest->format = pixelformat_sdl3_to_mortar(closest_sdl3.format);
	return true;
}

MORTAR_DisplayID MORTAR_GetDisplayForWindow(MORTAR_Window* mortar_window)
{
	return (MORTAR_DisplayID) SDL_GetDisplayForWindow(window_mortar_to_sdl3(mortar_window));
}

MORTAR_Window* MORTAR_EX_CreateWindow(MORTAR_EX_CreateWindowProps* createProps)
{
	if (!createProps) {
		SDL_SetError("createProps");
		return nullptr;
	}
	MORTAR_Window* mortar_window = (MORTAR_Window*) SDL_calloc(1, sizeof(MORTAR_Window));
	if (!mortar_window) {
		SDL_SetError("new MORTAR_Window");
		return nullptr;
	}
	SDL_PropertiesID props = SDL_CreateProperties();
	if (!props) {
		SDL_free(mortar_window);
		return nullptr;
	}
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, createProps->width);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, createProps->height);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, createProps->fullscreen);
	SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, createProps->title);
	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, createProps->hidden);
	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, createProps->opengl.enabled);
	if (createProps->opengl.enabled) {
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, createProps->opengl.doublebuffer);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, createProps->opengl.depth_size);
	}
	SDL_Window* sdl3_window = SDL_CreateWindowWithProperties(props);
	SDL_DestroyProperties(props);
	if (!sdl3_window) {
		SDL_free(mortar_window);
		return nullptr;
	}
	mortar_window->sdl3_window = sdl3_window;
	return mortar_window;
}

void MORTAR_DestroyWindow(MORTAR_Window* mortar_window)
{
	if (!mortar_window) {
		return;
	}
	SDL_DestroyWindow(mortar_window->sdl3_window);
	SDL_free(mortar_window);
}

bool MORTAR_EXT_SetWindowProperty(MORTAR_Window* mortar_window, MORTAR_WindowProperty key, void* prop)
{
	switch (key) {
	case MORTAR_WINDOW_PROPERTY_USER:
		mortar_window->user_property = prop;
		return true;
	default:
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "MORTAR_EXT_SetWindowProperty(%d): unsupported", key);
		return false;
	}
}

void* MORTAR_EXT_GetWindowProperty(MORTAR_Window* mortar_window, MORTAR_WindowProperty key, void* default_value)
{
	switch (key) {
	case MORTAR_WINDOW_PROPERTY_USER:
		return mortar_window->user_property;
	case MORTAR_WINDOW_PROPERTY_SDL3WINDOW:
		return mortar_window->sdl3_window;
	case MORTAR_WINDOW_PROPERTY_HWND:
		return SDL_GetPointerProperty(
			SDL_GetWindowProperties(window_mortar_to_sdl3(mortar_window)),
			SDL_PROP_WINDOW_WIN32_HWND_POINTER,
			nullptr
		);
	default:
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "MORTAR_EXT_GetWindowProperty: unsupported property");
		return default_value;
	}
}
