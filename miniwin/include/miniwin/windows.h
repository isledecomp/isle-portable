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
#define HWND_NOTOPMOST ((HWND) (-2))
#define RGB(r, g, b) (((BYTE) (r) | ((BYTE) (g) << 8) | ((BYTE) (b) << 16)))
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
#define RC_PALETTE 0x0100
#define SIZEPALETTE 104
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
typedef int* LPLONG;
typedef void* LPVOID;
typedef char* LPSTR;
typedef const char* LPCSTR;
typedef void* HANDLE;
typedef HANDLE HICON, HFONT;
typedef struct HINSTANCE__* HINSTANCE;
typedef SDL_Window *HMENU, *HWND;
typedef HANDLE HMODULE, HDC, HPALETTE, HFILE, HCURSOR;
typedef int LSTATUS, HKEY, REGSAM;

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
	LONG biXPelsPerMeter;
	LONG biYPelsPerMeter;
	DWORD biClrUsed;
	DWORD biClrImportant;
};
static_assert(sizeof(BITMAPINFOHEADER) == 40, "Incorrect size");
typedef BITMAPINFOHEADER* LPBITMAPINFOHEADER;

struct RGBQUAD {
	BYTE rgbBlue;
	BYTE rgbGreen;
	BYTE rgbRed;
	BYTE rgbReserved;
};

#pragma pack(push, 1)
struct BITMAPFILEHEADER {
	WORD bfType;
	DWORD bfSize;
	WORD bfReserved1;
	WORD bfReserved2;
	DWORD bfOffBits;
};
#pragma pack(pop)

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

BOOL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags);

HDC WINAPI GetDC(HWND hWnd);

int WINAPI ReleaseDC(HWND hWnd, HDC hDC);

int WINAPI GetDeviceCaps(HDC hdc, int index);

BOOL RedrawWindow(void* hWnd, const void* lprcUpdate, void* hrgnUpdate, unsigned int flags);

int SetBkColor(void*, int);

int SetBkMode(void*, int);

int SetTextColor(HDC hdc, int color);

BOOL GetTextExtentPoint(HDC hdc, LPCSTR lpString, int c, SIZE* psizl);

int ExtTextOut(HDC, int, int, unsigned int, const RECT*, LPCSTR, unsigned int, void*);

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
);

void* SelectObject(HDC, HFONT);

int GetTextExtentPoint32(HDC hdc, LPCSTR str, int len, SIZE* out);

HMENU GetMenu(HWND hWnd);

int DrawMenuBar(void* hWnd);

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
);

LONG GetWindowLong(HWND hWnd, int nIndex);

LONG SetWindowLong(HWND hWnd, int nIndex, LONG dwNewLong);

int DeleteObject(void*);

BOOL AdjustWindowRectEx(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle);

int SetRect(RECT* rc, int left, int top, int right, int bottom);

VOID WINAPI Sleep(DWORD dwMilliseconds);

BOOL ClientToScreen(HWND hWnd, LPPOINT lpPoint);
