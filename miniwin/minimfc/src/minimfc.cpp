#include "minimfc.h"

#include "miniwin.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdlib.h>
#include <vector>

char* afxCurrentAppName;
AFX_MODULE_STATE g_CustomModuleState;
CWinApp* wndTop;

SDL_Window* window;
const char* title = "Configure LEGO Island";

CWinApp::CWinApp()
{
	if (wndTop != NULL) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "There can only be one CWinApp!");
		abort();
	}
	wndTop = this;

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK)) {
		SDL_Log("SDL_Init: %s\n", SDL_GetError());
		return;
	}

	window = SDL_CreateWindow(title, 640, 480, 0);
}

int CWinApp::ExitInstance()
{
	SDL_Quit();
	return 0;
}

static char* get_base_filename(const char* path)
{
	for (;;) {
		const char* next = SDL_strpbrk(path, "/\\");
		if (next == NULL) {
			break;
		}
		path = next + 1;
	}
	const char* end = SDL_strchr(path, '.');
	size_t len;
	if (end == NULL) {
		len = SDL_strlen(path);
	}
	else {
		len = end - path;
	}
	char* filename = new char[len + 1];
	SDL_memcpy(filename, path, len);
	filename[len] = '\0';
	return filename;
}

int main(int argc, char* argv[])
{
	afxCurrentAppName = get_base_filename(argv[0]);
	if (wndTop == NULL) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No CWinApp created");
		abort();
	}
	wndTop->InitInstance();

	SDL_Event event;
	bool running = true;
	while (running) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				running = false;
			}
		}

		SDL_Delay(16); // 60 FPS
	}

	int result = wndTop->ExitInstance();
	free(afxCurrentAppName);
	return result;
}

void AfxMessageBox(const char* message)
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, NULL);
}

BOOL GetWindowRect(HWND hWnd, RECT* Rect)
{
	int x, y, w, h;
	if (!Rect) {
		return FALSE;
	}
	if (hWnd == NULL) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unregistered HWND %p", hWnd);
		Rect->left = 0;
		Rect->top = 0;
		Rect->right = 640;
		Rect->bottom = 480;
		return FALSE;
	}
	SDL_GetWindowPosition(hWnd, &x, &y);
	SDL_GetWindowSize(hWnd, &w, &h);

	Rect->right = x;
	Rect->top = y;
	Rect->left = w + x;
	Rect->bottom = h + y;

	return TRUE;
}

BOOL GetVersionEx(OSVERSIONINFOA* version)
{
	version->dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
	version->dwMajorVersion = 5; // Win2k/XP
	version->dwPlatformId = 2;   // NT
	version->dwBuildNumber = 0x500;
	return TRUE;
}

void OutputDebugString(const char* lpOutputString)
{
	SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "%s", lpOutputString);
}

int miniwin_stricmp(const char* str1, const char* str2)
{
	return SDL_strcasecmp(str1, str2);
}

void GlobalMemoryStatus(MEMORYSTATUS* memory_status)
{
	memory_status->dwLength = sizeof(*memory_status);
	memory_status->dwTotalPhys = 1024 * SDL_GetSystemRAM();
}
