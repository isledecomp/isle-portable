#include "miniwin.h"

#include "miniwin_p.h"

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
	SDL_LogError(LOG_CATEGORY_MINIWIN, "IUnknown does not implement guid");
	return E_NOINTERFACE;
}

BOOL SetWindowPos(HWND hWnd, int hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
{
	if (!hWnd) {
		return FALSE;
	}

	if (!(uFlags & SWP_NOACTIVATE)) {
		SDL_RaiseWindow(hWnd);
	}

	if (!(uFlags & SWP_NOSIZE)) {
		SDL_SetWindowSize(hWnd, cx, cy);
	}

	if (!(uFlags & SWP_NOMOVE)) {
		SDL_SetWindowPosition(hWnd, X, Y);
	}

	return TRUE;
}

VOID WINAPI Sleep(DWORD dwMilliseconds)
{
	SDL_Delay(dwMilliseconds);
}
