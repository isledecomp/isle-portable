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
	MINIWIN_NOT_IMPLEMENTED();
	return E_NOINTERFACE;
}

BOOL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
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

int SetBkColor(void*, int)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

int SetBkMode(void*, int)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

int SetTextColor(HDC hdc, int color)
{
	MINIWIN_NOT_IMPLEMENTED();
	return color;
}

BOOL GetTextExtentPoint(HDC hdc, LPCSTR lpString, int c, SIZE* psizl)
{
	MINIWIN_NOT_IMPLEMENTED();
	if (psizl) {
		psizl->cx = 8 * c;
		psizl->cy = 16;
	}
	return TRUE;
}

int ExtTextOut(HDC, int, int, unsigned int, const RECT*, LPCSTR, unsigned int, void*)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 1;
}

HFONT CreateFont(
	int,
	int,
	int,
	int,
	int,
	unsigned int,
	unsigned int,
	unsigned int,
	unsigned int,
	unsigned int,
	unsigned int,
	unsigned int,
	unsigned int,
	LPCSTR
)
{
	MINIWIN_NOT_IMPLEMENTED();
	return nullptr;
}

void* SelectObject(HDC, HFONT)
{
	MINIWIN_NOT_IMPLEMENTED();
	return nullptr;
}

int GetTextExtentPoint32(HDC hdc, LPCSTR str, int len, SIZE* out)
{
	return GetTextExtentPoint(hdc, str, len, out);
}

HMENU GetMenu(HWND hWnd)
{
	MINIWIN_NOT_IMPLEMENTED();
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
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

LONG SetWindowLong(HWND hWnd, int nIndex, LONG dwNewLong)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

int DeleteObject(void*)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 1;
}

BOOL AdjustWindowRectEx(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle)
{
	MINIWIN_NOT_IMPLEMENTED();
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
	MINIWIN_NOT_IMPLEMENTED();
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
