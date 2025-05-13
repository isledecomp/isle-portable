#include "minimfc.h"

#include "miniwin.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdlib.h>

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
}

CWinApp::~CWinApp() = default;

BOOL CWinApp::InitInstance()
{
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK)) {
		SDL_Log("SDL_Init: %s\n", SDL_GetError());
		return FALSE;
	}

	window = SDL_CreateWindow(title, 640, 480, 0);

	return TRUE;
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
	if (!wndTop->InitInstance()) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "CWinApp::InitInstance failed");
	}

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
