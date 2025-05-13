#include "miniwin.h"

#include <SDL3/SDL.h>
#include <utility>
#include <vector>

static std::vector<std::pair<SDL_Window*, HWND>> sdl_hwnd_mapping;

SDL_Window* miniwin_GetSdlWindow(HWND hWnd)
{
	for (size_t i = 0; i < sdl_hwnd_mapping.size(); i++) {
		if (sdl_hwnd_mapping[i].second == hWnd) {
			return sdl_hwnd_mapping[i].first;
		}
	}
	return NULL;
}

void OutputDebugString(const char* lpOutputString)
{
	SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "%s", lpOutputString);
}

VOID WINAPI Sleep(DWORD dwMilliseconds)
{
	SDL_Delay(dwMilliseconds);
}

BOOL GetWindowRect(HWND hWnd, tagRECT* Rect)
{
	int x, y, w, h;
	if (!Rect) {
		return FALSE;
	}
	SDL_Window* window = miniwin_GetSdlWindow(hWnd);
	if (window == NULL) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unregistered HWND %p", hWnd);
		Rect->left = 0;
		Rect->top = 0;
		Rect->right = 640;
		Rect->bottom = 480;
		return FALSE;
	}
	SDL_GetWindowPosition(window, &x, &y);
	SDL_GetWindowSize(window, &w, &h);

	Rect->right = x;
	Rect->top = y;
	Rect->left = w + x;
	Rect->bottom = h + y;

	return TRUE;
}

int _stricmp(const char* str1, const char* str2)
{
	return SDL_strcasecmp(str1, str2);
}

void GlobalMemoryStatus(MEMORYSTATUS* memory_status)
{
	memory_status->dwLength = sizeof(*memory_status);
	memory_status->dwTotalPhys = 1024 * SDL_GetSystemRAM();
}
