#pragma once

#include "miniwin.h"

struct CWnd {
	void* m_hWnd;
	void EnableWindow(bool bEnable) {}
	void SetWindowText(const char* text) {}
};

struct CPaintDC {
	void* m_hDC;
	CPaintDC(HWND hWnd) {}
	void Draw() {}
};

struct CDataExchange {
	bool m_bSaveAndValidate;
};

struct CDialog {
	void* m_hWnd;
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
	virtual ~CWinApp();
	virtual BOOL InitInstance();
	virtual int ExitInstance();
};

struct CCommandLineInfo {
	virtual void ParseParam(LPCSTR pszParam, BOOL bFlag, BOOL bLast) {}
};

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
