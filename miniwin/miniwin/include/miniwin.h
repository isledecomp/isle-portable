#pragma once

#include <SDL3/SDL_video.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

// --- Defines and Macros ---
#define MAKE_HRESULT(sev, fac, code)                                                                                   \
	((HRESULT) (((uint32_t) (sev) << 31) | ((uint32_t) (fac) << 16) | ((uint32_t) (code))))

#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8)                                                   \
	const GUID name = {l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}}

// Remove WinAPI stuff
#define CALLBACK
#define FAR
#define WINAPI
#define HWND_NOTOPMOST (HWND) - 2
#define RGB(r, g, b) ((r) | ((g) << 8) | ((b) << 16))
#define S_OK ((HRESULT) 0)
#define E_NOINTERFACE (0x80004002)
#define VOID void
#define TRUE 1
#define FALSE 0

#define SWP_NOACTIVATE 0x0010
#define SWP_NOMOVE 0x0002
#define SWP_NOSIZE 0x0004
#define SWP_NOZORDER 0x0001

#define WM_CLOSE 0x0010
#define WM_QUIT 0x0012
#define WM_TIMER 0x0113

#define WS_CAPTION 0x00C00000L
#define WS_OVERLAPPED 0x00000000L
#define WS_POPUP 0x80000000L
#define WS_SYSMENU 0x00080000L
#define WS_THICKFRAME 0x00040000L

#define GWL_STYLE (-16)
#define GWL_EXSTYLE -20

#define ANSI_CHARSET 0
#define BI_RGB 0
#define CLIP_DEFAULT_PRECIS 0
#define DEFAULT_QUALITY 0
#define ETO_OPAQUE 0x0002
#define FF_DONTCARE 0x00000000
#define RASTERCAPS 0x00000000
#define RC_PALETTE 0x00000000
#define SIZEPALETTE 256
#define FW_NORMAL 400
#define OPAQUE 2
#define OUT_DEFAULT_PRECIS 0
#define RDW_FRAME 0x0400
#define SRCCOPY 0x00CC0020
#define VARIABLE_PITCH 2

// --- Typedefs ---
typedef uint8_t BYTE, byte;
typedef int32_t LONG;
typedef uint32_t ULONG, DWORD, HRESULT;
typedef DWORD* LPDWORD;
typedef int BOOL, WINBOOL, INT;
typedef unsigned int UINT;
typedef unsigned short WORD;
typedef long* LPLONG;
typedef void* LPVOID;
typedef char* LPSTR;
typedef const char* LPCSTR;
typedef void* HANDLE;
typedef HANDLE HICON, HFONT;
typedef struct HINSTANCE__* HINSTANCE;
typedef SDL_Window *HMENU, *HWND;
typedef HANDLE HMODULE, HDC, HPALETTE, HFILE, HCURSOR;
typedef LONG LSTATUS, HKEY, REGSAM;

// --- Structs ---
struct tagPOINT {
	LONG x;
	LONG y;
};
typedef tagPOINT* LPPOINT;

struct SIZE {
	LONG cx;
	LONG cy;
};

struct RECT {
	LONG left;
	LONG top;
	LONG right;
	LONG bottom;
};
typedef RECT* LPRECT;

struct BITMAPINFOHEADER {
	DWORD biSize;
	LONG biWidth;
	LONG biHeight;
	WORD biPlanes;
	WORD biBitCount;
	DWORD biCompression;
	DWORD biSizeImage;
};
typedef BITMAPINFOHEADER* LPBITMAPINFOHEADER;

struct RGBQUAD {
	BYTE rgbBlue;
	BYTE rgbGreen;
	BYTE rgbRed;
	BYTE rgbReserved;
};

struct BITMAPFILEHEADER {
	WORD bfType;
	DWORD bfSize;
	WORD bfReserved1;
	WORD bfReserved2;
	DWORD bfOffBits;
};

struct BITMAPINFO {
	BITMAPINFOHEADER bmiHeader;
	RGBQUAD bmiColors[1];
};

struct GUID {
	uint32_t m_data1;
	uint16_t m_data2;
	uint16_t m_data3;
	uint8_t m_data4[8];
};
typedef GUID* LPGUID;

struct IUnknown {
	IUnknown() : m_refCount(1) {}
	virtual ULONG AddRef();
	virtual ULONG Release();
	virtual HRESULT QueryInterface(const GUID& riid, void** ppvObject);
	virtual ~IUnknown() = default;

protected:
	int m_refCount;
};

inline BOOL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
{
	return TRUE;
}

inline HDC WINAPI GetDC(HWND hWnd)
{
	return 0;
}

inline int WINAPI ReleaseDC(HWND hWnd, HDC hDC)
{
	return 0;
}

inline int WINAPI GetDeviceCaps(HDC hdc, int index)
{
	return 0;
}

inline BOOL RedrawWindow(void* hWnd, const void* lprcUpdate, void* hrgnUpdate, unsigned int flags)
{
	return 1;
}

inline int SetBkColor(void*, int)
{
	return 0;
}

inline int SetBkMode(void*, int)
{
	return 0;
}

inline int SetTextColor(HDC hdc, int color)
{
	return color;
}

inline static BOOL GetTextExtentPoint(HDC hdc, LPCSTR lpString, int c, SIZE* psizl)
{
	if (psizl) {
		psizl->cx = 8 * c;
		psizl->cy = 16;
	}
	return TRUE;
}

inline int ExtTextOut(HDC, int, int, unsigned int, const RECT*, LPCSTR, unsigned int, void*)
{
	return 1;
}

inline HFONT CreateFont(
	int,
	int,
	int,
	int,
	int,
	unsigned long,
	unsigned long,
	unsigned long,
	unsigned long,
	unsigned long,
	unsigned long,
	unsigned long,
	unsigned long,
	LPCSTR
)
{
	return nullptr;
}

inline void* SelectObject(HDC, HFONT)
{
	return nullptr;
}

inline int GetTextExtentPoint32(HDC hdc, LPCSTR str, int len, SIZE* out)
{
	return GetTextExtentPoint(hdc, str, len, out);
}

inline HMENU GetMenu(HWND hWnd)
{
	return NULL;
}

inline int DrawMenuBar(void* hWnd)
{
	return 1;
}

inline static int StretchDIBits(
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
	return 0;
}

inline LONG GetWindowLong(HWND hWnd, int nIndex)
{
	return 0;
}

inline LONG SetWindowLong(HWND hWnd, int nIndex, LONG dwNewLong)
{
	return 0;
}

inline int DeleteObject(void*)
{
	return 1;
}

inline BOOL AdjustWindowRectEx(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle)
{
	return TRUE;
}

inline int SetRect(RECT* rc, int left, int top, int right, int bottom)
{
	rc->left = left;
	rc->top = top;
	rc->right = right;
	rc->bottom = bottom;
	return 1;
}

VOID WINAPI Sleep(DWORD dwMilliseconds);

inline BOOL ClientToScreen(HWND hWnd, LPPOINT lpPoint)
{
	return TRUE;
}
