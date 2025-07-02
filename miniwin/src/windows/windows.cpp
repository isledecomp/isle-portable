#include "miniwin.h"
#include "miniwin/ddraw.h"

#include <SDL3/SDL.h>
#include <vector>

ULONG IUnknown::AddRef()
{
	m_refCount += 1;
	return m_refCount;
}

ULONG IUnknown::Release()
{
	m_refCount -= 1;
	if (m_refCount == 0) {
		delete this;
		return 0;
	}
	return m_refCount;
}

HRESULT IUnknown::QueryInterface(const GUID& riid, void** ppvObject)
{
	MINIWIN_NOT_IMPLEMENTED();
	return E_NOINTERFACE;
}

HRESULT IDirectDrawClipper::SetHWnd(DWORD unnamedParam1, HWND hWnd)
{
	return DD_OK;
}

BOOL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
{
	if (!hWnd) {
		return FALSE;
	}
	SDL_Window* sdlWindow = reinterpret_cast<SDL_Window*>(hWnd);

	if (!(uFlags & SWP_NOACTIVATE)) {
		SDL_RaiseWindow(sdlWindow);
	}

	if (!(uFlags & SWP_NOSIZE)) {
		SDL_SetWindowSize(sdlWindow, cx, cy);
	}

	if (!(uFlags & SWP_NOMOVE)) {
		SDL_SetWindowPosition(sdlWindow, X, Y);
	}

	return TRUE;
}

HDC WINAPI GetDC(HWND hWnd)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

int WINAPI ReleaseDC(HWND hWnd, HDC hDC)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

int WINAPI GetDeviceCaps(HDC hdc, int index)
{

	if (index == RASTERCAPS) {
		return 0;
	}
	if (index == SIZEPALETTE) {
		return 256;
	}
	return 0;
}

BOOL RedrawWindow(void* hWnd, const void* lprcUpdate, void* hrgnUpdate, unsigned int flags)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 1;
}

HMENU GetMenu(HWND hWnd)
{
	return NULL;
}

int DrawMenuBar(void* hWnd)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 1;
}

int StretchDIBits(
	void* hdc,
	int xDest,
	int yDest,
	int DestWidth,
	int DestHeight,
	int xSrc,
	int ySrc,
	int SrcWidth,
	int SrcHeight,
	const void* lpBits,
	const void* lpbmi,
	unsigned int iUsage,
	uint32_t rop
)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

LONG GetWindowLong(HWND hWnd, int nIndex)
{
	SDL_Window* sdlWindow = reinterpret_cast<SDL_Window*>(hWnd);
	if (nIndex == GWL_STYLE) {
		Uint32 flags = SDL_GetWindowFlags(sdlWindow);
		LONG style = WS_POPUP;
		if ((flags & SDL_WINDOW_BORDERLESS) == 0) {
			style = WS_OVERLAPPED | WS_CAPTION;
			if (flags & SDL_WINDOW_RESIZABLE) {
				style |= WS_THICKFRAME;
			}
		}
		return style;
	}
	else if (nIndex == GWL_EXSTYLE) {
		return 0;
	}

	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

LONG SetWindowLong(HWND hWnd, int nIndex, LONG dwNewLong)
{
	SDL_Window* sdlWindow = reinterpret_cast<SDL_Window*>(hWnd);
	if (nIndex == GWL_STYLE) {
		SDL_SetWindowBordered(sdlWindow, (dwNewLong & WS_CAPTION) != 0);
		SDL_SetWindowResizable(sdlWindow, (dwNewLong & WS_THICKFRAME) != 0);

		return dwNewLong;
	}

	return 0;
}

int DeleteObject(void*)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 1;
}

BOOL AdjustWindowRectEx(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle)
{
	return TRUE;
}

int SetRect(RECT* rc, int left, int top, int right, int bottom)
{
	rc->left = left;
	rc->top = top;
	rc->right = right;
	rc->bottom = bottom;
	return 1;
}

VOID WINAPI Sleep(DWORD dwMilliseconds)
{
	SDL_Delay(dwMilliseconds);
}

BOOL ClientToScreen(HWND hWnd, LPPOINT lpPoint)
{
	return TRUE;
}

int _chdir(char* path)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

intptr_t _spawnl(
	int mode,
	const char* cmdname,
	const char* arg0,
	const char* arg1,
	const char* arg2,
	const char* arg3,
	...
)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

UINT WINAPI GetSystemPaletteEntries(HDC hdc, UINT iStart, UINT cEntries, LPPALETTEENTRY pPalEntries)
{
	MINIWIN_NOT_IMPLEMENTED();
	for (UINT i = 0; i < cEntries; i++) {
		UINT val = iStart + i;
		pPalEntries[i].peRed = val;
		pPalEntries[i].peGreen = val;
		pPalEntries[i].peBlue = val;
		pPalEntries[i].peFlags = PC_NONE;
	}
	return cEntries;
}

HPALETTE CreatePalette(LPLOGPALETTE lpLogPalette)
{
	MINIWIN_NOT_IMPLEMENTED();
	return nullptr;
}

int SelectPalette(HDC hdc, HPALETTE hpal, BOOL bForceBackground)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

int RealizePalette(HDC hdc)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}
