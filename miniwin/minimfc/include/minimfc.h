#pragma once

#include "miniwin.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

// --- Defines and Macros ---

// Remove WinAPI stuff
#define BEGIN_MESSAGE_MAP(class_name, base_class_name)
#define DECLARE_MESSAGE_MAP()
#ifndef __cdecl
#define __cdecl
#endif
#define END_MESSAGE_MAP()
#define ON_COMMAND(id, func)
#define ON_LBN_SELCHANGE(id, func)
#define ON_WM_DESTROY()
#define ON_WM_PAINT()
#define ON_WM_QUERYDRAGICON()
#define ON_WM_SYSCOMMAND()

#define FAILED(hr) (((HRESULT) (hr)) < 0)
#define InterlockedIncrement(x) __sync_add_and_fetch(x, 1)
#define INVALID_HANDLE ((HANDLE) -1)
#define INVALID_HANDLE_VALUE ((HANDLE) -1)
#define HKEY_LOCAL_MACHINE ((HKEY) 0x80000002)
#define LOWORD(l) ((WORD) (((DWORD_PTR) (l)) & 0xffff))
#define MAKEINTRESOURCE(i) (reinterpret_cast<LPCTSTR>((ULONG_PTR) ((WORD) (i))))

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

#define WM_ICONERASEBKGND 0x0027
#define WM_SETICON 0x804

#define ERROR_SUCCESS 0
#define GENERIC_READ 0x80000000L
#define OPEN_EXISTING 3
#define REG_SZ 1
#define RT_GROUP_ICON 0x00000007
#define SW_RESTORE 9

// --- Typedefs ---
typedef char CHAR;
typedef BYTE* LPBYTE;
typedef char* LPTSTR;
typedef const char* LPCTSTR;
typedef intptr_t INT_PTR, *PINT_PTR;
typedef uintptr_t UINT_PTR, *PUINT_PTR;
typedef intptr_t LONG_PTR, *PLONG_PTR;
typedef uintptr_t ULONG_PTR, *PULONG_PTR;
typedef ULONG_PTR DWORD_PTR, *PDWORD_PTR;
typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM, LRESULT;
typedef HKEY* PHKEY;
typedef BITMAPINFO* LPBITMAPINFO;
typedef GUID REFIID;

// --- Structs ---
struct OSVERSIONINFOA {
	DWORD dwOSVersionInfoSize;
	DWORD dwMajorVersion;
	DWORD dwBuildNumber;
	DWORD dwPlatformId;
};
typedef struct IUnknown* LPUNKNOWN;

struct CWnd {
	HWND m_hWnd;
	void EnableWindow(bool bEnable) {}
	void SetWindowText(const char* text) {}
};

struct CDataExchange {
	bool m_bSaveAndValidate;
};

struct CDialog {
	HWND m_hWnd;
	int m_nIDTemplate;
	CWnd* m_pParentWnd;
	CDialog() : m_nIDTemplate(0), m_pParentWnd(nullptr) {}
	CDialog(int nIDTemplate) : m_nIDTemplate(nIDTemplate), m_pParentWnd(nullptr) {}
	CDialog(int nIDTemplate, CWnd* pParent) : m_nIDTemplate(nIDTemplate), m_pParentWnd(pParent) {}
	virtual BOOL OnInitDialog() { return TRUE; }
	void OnCancel() {}
	virtual void OnOK() {}
	virtual void DoModal() {}
	virtual void Default() {}
	virtual void EndDialog(int nResult) {}
	virtual void DoDataExchange(CDataExchange* pDX) {}
};

struct CPaintDC {
	void* m_hDC;
	CPaintDC(CDialog* hWnd) {}
	void Draw() {}
};

struct CMenu {
	void* m_hMenu;
	CMenu() : m_hMenu(nullptr) {}
	static CMenu* FromHandle(void* hMenu)
	{
		CMenu* pMenu = new CMenu();
		pMenu->m_hMenu = hMenu;
		return pMenu;
	}
	bool InsertMenu(UINT uPosition, UINT uFlags, UINT_PTR uIDNewItem, LPCTSTR lpszNewItem) { return true; }
	bool RemoveMenu(UINT uPosition, UINT uFlags) { return true; }
	bool SetMenuItemInfo(UINT uIDItem, const void* pMenuItemInfo, bool fByPosition = false) { return true; }
	int GetMenuItemCount() const { return 0; }
};

struct CWinApp {
	CWinApp();
	~CWinApp() override = default;
	virtual BOOL InitInstance() = 0;
	virtual int ExitInstance();
};

struct MEMORYSTATUS {
	DWORD dwLength;
	DWORD dwTotalPhys;
};

struct CCommandLineInfo {
	virtual void ParseParam(LPCSTR pszParam, BOOL bFlag, BOOL bLast) {}
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

inline BOOL SetWindowPos(CWinApp** hWnd, int X, int Y, int cx, int cy, UINT uFlags)
{
	return TRUE;
}

BOOL GetWindowRect(HWND hDlg, struct RECT* Rect);

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

BOOL GetVersionEx(OSVERSIONINFOA* version);

void GlobalMemoryStatus(MEMORYSTATUS* memory_status);

inline BOOL DrawIcon(HDC hdc, int x, int y, HICON hIcon)
{
	return TRUE;
}

inline bool AppendMenu(void* menu, UINT uFlags, UINT_PTR uIDNewItem, LPCTSTR lpszNewItem)
{
	return true;
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

int miniwin_stricmp(const char* str1, const char* str2);
#define _stricmp miniwin_stricmp

inline HICON LoadIcon(HINSTANCE hInstance, LPCSTR lpIconName)
{
	return 0;
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

inline BOOL IsDlgButtonChecked(int nIDButton)
{
	return 0;
}

inline CWnd* GetDlgItem(int id)
{
	return new CWnd();
}

inline BOOL OnInitDialog(HWND hDlg, HWND hwndFocus, LPARAM lParam)
{
	return TRUE;
}

inline BOOL CheckRadioButton(int nIDFirstButton, int nIDLastButton, int nIDCheckButton)
{
	return TRUE;
}

inline BOOL CheckDlgButton(int nIDButton, BOOL uCheck)
{
	return TRUE;
}

inline void Enable3dControls()
{
}

inline void ParseCommandLine(CCommandLineInfo& cmdInfo)
{
}

struct AFX_MODULE_STATE {
	CWinApp* m_pCurrentWinApp;
};
extern char* afxCurrentAppName;
extern CWinApp* wndTop;
extern AFX_MODULE_STATE g_CustomModuleState;
#define afxCurrentWinApp AfxGetModuleState()->m_pCurrentWinApp
inline AFX_MODULE_STATE* AfxGetModuleState()
{
	g_CustomModuleState.m_pCurrentWinApp = wndTop;
	return &g_CustomModuleState;
}

void AfxMessageBox(const char* message);
