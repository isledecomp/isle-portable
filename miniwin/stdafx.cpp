#include "../CONFIG/config.h"
#include "miniwin.h"

#include <SDL3/SDL.h>

SDL_Window* window;
const char* title = "Configure LEGO Island";

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

CWinApp::~CWinApp() = default;

const char* afxCurrentAppName = "";
AFX_MODULE_STATE g_CustomModuleState;
CWinApp* wndTop;

int main(int argc, char* argv[])
{
	CWinApp base;
	if (!base.InitInstance()) {
		return 1;
	}

	CConfigApp app;
	wndTop = &app;
	if (!app.InitInstance()) {
		return 1;
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

	return base.ExitInstance() + app.ExitInstance();
}

BOOL GetWindowRect(HWND hDlg, tagRECT* Rect)
{
	int x, y, w, h;
	SDL_GetWindowPosition(window, &x, &y);
	SDL_GetWindowSize(window, &w, &h);

	Rect->right = x;
	Rect->top = y;
	Rect->left = w + x;
	Rect->bottom = h + y;

	return TRUE;
}

void AfxMessageBox(const char* message)
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, NULL);
}
