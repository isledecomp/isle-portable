#pragma once

#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// --- Defines and Macros ---
#define RASTERCAPS 0x00000000
#define RC_PALETTE 0x00000000
#define SIZEPALETTE 256
#define MAKE_HRESULT(sev, fac, code)                                                                                   \
	((HRESULT) (((uint32_t) (sev) << 31) | ((uint32_t) (fac) << 16) | ((uint32_t) (code))))

// This is not the right way to make a GUID
#define DEFINE_GUID(GuidName, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8)                                               \
	const GUID GuidName = {l, w1, w2, b1 + b2 + b3 + b4 + b5 + b6 + b7 + b8}

// Remove WinAPI stuff
#define BEGIN_MESSAGE_MAP(class_name, base_class_name)
#define DECLARE_MESSAGE_MAP()
#define CALLBACK
#ifndef __cdecl
#define __cdecl
#endif
#define FAR
#define END_MESSAGE_MAP()
#define ON_COMMAND(id, func)
#define ON_LBN_SELCHANGE(id, func)
#define ON_WM_DESTROY()
#define ON_WM_PAINT()
#define ON_WM_QUERYDRAGICON()
#define ON_WM_SYSCOMMAND()
#define WINAPI

#define FAILED(hr) (((HRESULT) (hr)) < 0)
#define InterlockedIncrement(x) __sync_add_and_fetch(x, 1)
#define INVALID_HANDLE ((HANDLE) - 1)
#define INVALID_HANDLE_VALUE ((HANDLE) - 1)
#define HKEY_LOCAL_MACHINE ((HKEY) 0x80000002)
#define GWL_STYLE (-16)
#define HWND_NOTOPMOST (HWND) - 2
#define LOWORD(l) ((WORD) (((DWORD_PTR) (l)) & 0xffff))
#define MAKEINTRESOURCE(i) (reinterpret_cast<LPCTSTR>((ULONG_PTR) ((WORD) (i))))
#define RGB(r, g, b) ((r) | ((g) << 8) | ((b) << 16))
#define S_OK ((HRESULT) 0)
#define E_NOINTERFACE (0x80004002)
#define VOID void
#define TRUE 1
#define FALSE 0

#define ICON_BIG 1
#define ICON_SMALL 0

#define KEY_READ 0x20019
#define KEY_WRITE 0x20006

#define LB_ADDSTRING 0x180
#define LB_GETCURSEL 0x0188
#define LB_SETCURSEL 0x185

#define MF_SEPARATOR 0x80000000
#define MF_STRING 0x00000000

#define SM_CXICON 11
#define SM_CYICON 12

#define SWP_NOACTIVATE 0x0010
#define SWP_NOMOVE 0x0002
#define SWP_NOSIZE 0x0004
#define SWP_NOZORDER 0x0001

#define WM_CLOSE 0x0010
#define WM_ICONERASEBKGND 0x0027
#define WM_SETICON 0x804
#define WM_QUIT 0x0012
#define WM_TIMER 0x0113

#define WS_CAPTION 0x00C00000L
#define WS_OVERLAPPED 0x00000000L
#define WS_POPUP 0x80000000L
#define WS_SYSMENU 0x00080000L
#define WS_THICKFRAME 0x00040000L

#define ANSI_CHARSET 0
#define BI_RGB 0
#define CLIP_DEFAULT_PRECIS 0
#define DEFAULT_QUALITY 0
#define ERROR_SUCCESS 0
#define ETO_OPAQUE 0x0002
#define FF_DONTCARE 0x00000000
#define FW_NORMAL 400
#define GENERIC_READ 0x80000000L
#define GWL_EXSTYLE -20
#define OPAQUE 2
#define OPEN_EXISTING 3
#define OUT_DEFAULT_PRECIS 0
#define RDW_FRAME 0x0400
#define REG_SZ 1
#define RT_GROUP_ICON 0x00000007
#define SRCCOPY 0x00CC0020
#define SW_RESTORE 9
#define VARIABLE_PITCH 2

// --- Typedefs ---
typedef char CHAR;
typedef uint8_t BYTE, byte;
typedef BYTE* LPBYTE;
typedef int32_t LONG;
typedef uint32_t ULONG, DWORD;
typedef DWORD* LPDWORD;
typedef int BOOL, WINBOOL, INT;
typedef unsigned int UINT;
typedef unsigned short WORD;
typedef long* LPLONG;
typedef void* LPVOID;
typedef char* LPSTR;
typedef char* LPTSTR;
typedef const char* LPCTSTR;
typedef const char* LPCSTR;
typedef intptr_t INT_PTR, *PINT_PTR;
typedef uintptr_t UINT_PTR, *PUINT_PTR;
typedef intptr_t LONG_PTR, *PLONG_PTR;
typedef uintptr_t ULONG_PTR, *PULONG_PTR;
typedef ULONG_PTR DWORD_PTR, *PDWORD_PTR;
typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM, LRESULT;
typedef void* HANDLE;
typedef HANDLE HMENU, HICON, HFONT;
typedef struct HINSTANCE__ * HINSTANCE;
typedef HANDLE HWND, HMODULE, HDC, HPALETTE, HFILE, HCURSOR;
typedef LONG HRESULT, LSTATUS, HKEY, REGSAM;
typedef HKEY* PHKEY;

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

struct tagRECT {
	LONG left;
	LONG top;
	LONG right;
	LONG bottom;
};
typedef tagRECT RECT;
typedef tagRECT* LPRECT;

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
typedef BITMAPINFO* LPBITMAPINFO;

struct PALETTEENTRY {
	BYTE peRed;
	BYTE peGreen;
	BYTE peBlue;
	BYTE peFlags;
};
typedef PALETTEENTRY* LPPALETTEENTRY;

struct GUID {
	int m_data1;
	int m_data2;
	int m_data3;
	int m_data4;
};
typedef GUID REFIID;
typedef GUID* LPGUID;

struct IUnknown {
	virtual ULONG AddRef() = 0;
	virtual ULONG Release() = 0;
	virtual HRESULT QueryInterface(const GUID& riid, void** ppvObject) = 0;
};
typedef struct IUnknown* LPUNKNOWN;

struct LOGPALETTE {
	WORD palVersion;
	WORD palNumEntries;
	PALETTEENTRY palPalEntry[1];
};
typedef LOGPALETTE* LPLOGPALETTE;

struct MEMORYSTATUS {
	DWORD dwLength;
	DWORD dwTotalPhys;
};

struct OSVERSIONINFOA {
	DWORD dwOSVersionInfoSize;
	DWORD dwMajorVersion;
	DWORD dwBuildNumber;
	DWORD dwPlatformId;
};

// --- Classs ---
class CString {
public:
	CString(const char* str = "") : m_str(str) {}
	void LoadString(int str) {}
	operator const char*() const { return m_str; }

private:
	const char* m_str;
};

// --- Functions ---
inline WINBOOL WINAPI SetForegroundWindow(HWND hWnd)
{
	return TRUE;
}

inline BOOL ShowWindow(HWND hWnd, int nCmdShow)
{
	return TRUE;
}

inline BOOL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
{
	return TRUE;
}

inline BOOL SetWindowPos(HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
{
	return TRUE;
}

BOOL GetWindowRect(HWND hDlg, struct tagRECT *Rect);

inline BOOL GetClientRect(LPRECT lpRect)
{
	return TRUE;
}

inline BOOL IsIconic()
{
	return FALSE;
}

inline int GetSystemMetrics(int nIndex)
{
	return 0;
}

inline UINT WINAPI GetSystemPaletteEntries(HDC hdc, UINT iStart, UINT cEntries, LPPALETTEENTRY pPalEntries)
{
	return 0;
}

inline BOOL GetVersionEx(OSVERSIONINFOA* version)
{
	return TRUE;
}

void GlobalMemoryStatus(MEMORYSTATUS* memory_status);

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

inline BOOL DrawIcon(HDC hdc, int x, int y, HICON hIcon)
{
	return TRUE;
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

inline bool AppendMenu(void* menu, UINT uFlags, UINT_PTR uIDNewItem, LPCTSTR lpszNewItem)
{
	return true;
}

inline HMENU GetMenu(HWND hWnd)
{
	return NULL;
}

inline int DrawMenuBar(void* hWnd)
{
	return 1;
}

inline LSTATUS RegOpenKeyEx(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult)
{
	return ERROR_SUCCESS;
}

inline LSTATUS RegQueryValueEx(
	HKEY hKey,
	LPCSTR lpValueName,
	LPDWORD lpReserved,
	LPDWORD lpType,
	BYTE* lpData,
	LPDWORD lpcbData
)
{
	return ERROR_SUCCESS;
}

inline LSTATUS RegCloseKey(HKEY hKey)
{
	return ERROR_SUCCESS;
}

inline LSTATUS RegSetValueEx(
	HKEY hKey,
	LPCSTR lpValueName,
	DWORD Reserved,
	DWORD dwType,
	const BYTE* lpData,
	DWORD cbData
)
{
	return ERROR_SUCCESS;
}

inline LSTATUS RegCreateKeyEx(
	HKEY hKey,
	LPCSTR lpSubKey,
	DWORD Reserved,
	const char* lpClass,
	DWORD dwOptions,
	REGSAM samDesired,
	void* lpSecurityAttributes,
	PHKEY phkResult,
	LPDWORD lpdwDisposition
)
{
	return ERROR_SUCCESS;
}

void OutputDebugString(const char* lpOutputString);

inline void* GetProcAddress(HMODULE module, const char* name)
{
	return 0;
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

int miniwin_stricmp(const char* str1, const char* str2);
#define _stricmp miniwin_stricmp

inline BOOL AdjustWindowRectEx(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle)
{
	return TRUE;
}

inline HICON LoadIcon(HINSTANCE hInstance, LPCSTR lpIconName)
{
	return 0;
}

inline int SetRect(RECT* rc, int left, int top, int right, int bottom)
{
	rc->left = left;
	rc->top = top;
	rc->right = right;
	rc->bottom = bottom;
	return 1;
}

inline int lstrcmpi(LPCSTR lpString1, LPCSTR lpString2)
{
	return 0;
}

inline HINSTANCE AfxFindResourceHandle(LPCTSTR lpszResourceName, int lpszResourceType)
{
	return 0;
}

inline HMODULE LoadLibrary(const char* name)
{
	return 0;
}

inline int FreeLibrary(void* hModule)
{
	return 0;
}

inline HMENU GetSystemMenu(HWND hWnd, bool bRevert)
{
	assert(false && "Needs implementation");
	return reinterpret_cast<HMENU>(0x1234);
}

VOID WINAPI Sleep(DWORD dwMilliseconds);

inline HWND WINAPI FindWindow(LPCSTR lpClassName, LPCSTR lpWindowName)
{
	return 0;
}

inline LRESULT SendMessage(UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return 0;
}

inline LRESULT SendMessage(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return 0;
}

inline HPALETTE CreatePalette(LPLOGPALETTE lpLogPalette)
{
	return nullptr;
}

inline int SelectPalette(HDC hdc, HPALETTE hpal, BOOL bForceBackground)
{
	return 0;
}

inline int RealizePalette(HDC hdc)
{
	return 0;
}

inline BOOL ClientToScreen(HWND hWnd, LPPOINT lpPoint)
{
	return TRUE;
}
