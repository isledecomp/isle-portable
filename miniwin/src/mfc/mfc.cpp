#include "miniwin/mfc.h"

#include "miniwin.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdlib.h>
#include <vector>

char* afxCurrentAppName;
AFX_MODULE_STATE g_CustomModuleState;
CWinApp* wndTop;

SDL_Window* window;
const char* title = "Configure LEGO Island";

void CWnd::EnableWindow(bool bEnable)
{
	MINIWIN_NOT_IMPLEMENTED();
}
void CWnd::SetWindowText(const char* text)
{
	MINIWIN_NOT_IMPLEMENTED();
}

CDialog::CDialog() : m_nIDTemplate(0), m_pParentWnd(nullptr)
{
	MINIWIN_NOT_IMPLEMENTED();
}

CDialog::CDialog(int nIDTemplate) : m_nIDTemplate(nIDTemplate), m_pParentWnd(nullptr)
{
	MINIWIN_NOT_IMPLEMENTED();
}

CDialog::CDialog(int nIDTemplate, CWnd* pParent) : m_nIDTemplate(nIDTemplate), m_pParentWnd(pParent)
{
	MINIWIN_NOT_IMPLEMENTED();
}

BOOL CDialog::OnInitDialog()
{
	MINIWIN_NOT_IMPLEMENTED();
	return TRUE;
}

void CDialog::OnCancel()
{
	MINIWIN_NOT_IMPLEMENTED();
}

void CDialog::OnOK()
{
	MINIWIN_NOT_IMPLEMENTED();
}

void CDialog::DoModal()
{
	MINIWIN_NOT_IMPLEMENTED();
}

void CDialog::Default()
{
	MINIWIN_NOT_IMPLEMENTED();
}

void CDialog::EndDialog(int nResult)
{
	MINIWIN_NOT_IMPLEMENTED();
}

void CDialog::DoDataExchange(CDataExchange* pDX)
{
	MINIWIN_NOT_IMPLEMENTED();
}

CPaintDC::CPaintDC(CDialog* hWnd)
{
	MINIWIN_NOT_IMPLEMENTED();
}

bool CMenu::InsertMenu(UINT uPosition, UINT uFlags, UINT_PTR uIDNewItem, LPCTSTR lpszNewItem)
{
	MINIWIN_NOT_IMPLEMENTED();
	return true;
}

bool CMenu::RemoveMenu(UINT uPosition, UINT uFlags)
{
	MINIWIN_NOT_IMPLEMENTED();
	return true;
}

bool CMenu::SetMenuItemInfo(UINT uIDItem, const void* pMenuItemInfo, bool fByPosition)
{
	MINIWIN_NOT_IMPLEMENTED();
	return true;
}

int CMenu::GetMenuItemCount() const
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

void CPaintDC::Draw()
{
	MINIWIN_NOT_IMPLEMENTED();
}

CWinApp::CWinApp()
{
	if (wndTop != NULL) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "There can only be one CWinApp!");
		abort();
	}
	wndTop = this;

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK)) {
		SDL_Log("SDL_Init: %s\n", SDL_GetError());
		return;
	}

	window = SDL_CreateWindow(title, 640, 480, 0);
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
	wndTop->InitInstance();

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
	delete[] afxCurrentAppName;
	return result;
}

void AfxMessageBox(const char* message)
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, NULL);
}

BOOL GetWindowRect(HWND hWnd, RECT* Rect)
{
	int x, y, w, h;
	if (!Rect) {
		return FALSE;
	}
	if (hWnd == NULL) {
		MINIWIN_NOT_IMPLEMENTED();
		return FALSE;
	}
	SDL_GetWindowPosition(hWnd, &x, &y);
	SDL_GetWindowSize(hWnd, &w, &h);

	Rect->right = x;
	Rect->top = y;
	Rect->left = w + x;
	Rect->bottom = h + y;

	return TRUE;
}

BOOL GetVersionEx(OSVERSIONINFOA* version)
{
	version->dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
	version->dwMajorVersion = 5; // Win2k/XP
	version->dwPlatformId = 2;   // NT
	version->dwBuildNumber = 0x500;
	return TRUE;
}

void OutputDebugString(const char* lpOutputString)
{
	SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "%s", lpOutputString);
}

void* GetProcAddress(HMODULE module, const char* name)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

int miniwin_stricmp(const char* str1, const char* str2)
{
	return SDL_strcasecmp(str1, str2);
}

void GlobalMemoryStatus(MEMORYSTATUS* memory_status)
{
	memory_status->dwLength = sizeof(*memory_status);
	memory_status->dwTotalPhys = 1024 * SDL_GetSystemRAM();
}

HICON LoadIcon(HINSTANCE hInstance, LPCSTR lpIconName)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

int lstrcmpi(LPCSTR lpString1, LPCSTR lpString2)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

HINSTANCE AfxFindResourceHandle(LPCTSTR lpszResourceName, int lpszResourceType)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

HMODULE LoadLibrary(const char* name)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

int FreeLibrary(void* hModule)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

HMENU GetSystemMenu(HWND hWnd, bool bRevert)
{
	MINIWIN_NOT_IMPLEMENTED();
	assert(false && "Needs implementation");
	return reinterpret_cast<HMENU>(0x1234);
}

HWND WINAPI FindWindow(LPCSTR lpClassName, LPCSTR lpWindowName)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

LRESULT SendMessage(UINT Msg, WPARAM wParam, LPARAM lParam)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

LRESULT SendMessage(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

BOOL IsDlgButtonChecked(int nIDButton)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

CWnd* GetDlgItem(int id)
{
	MINIWIN_NOT_IMPLEMENTED();
	return new CWnd();
}

BOOL OnInitDialog(HWND hDlg, HWND hwndFocus, LPARAM lParam)
{
	MINIWIN_NOT_IMPLEMENTED();
	return TRUE;
}

BOOL CheckRadioButton(int nIDFirstButton, int nIDLastButton, int nIDCheckButton)
{
	MINIWIN_NOT_IMPLEMENTED();
	return TRUE;
}

BOOL CheckDlgButton(int nIDButton, BOOL uCheck)
{
	MINIWIN_NOT_IMPLEMENTED();
	return TRUE;
}

void Enable3dControls()
{
	MINIWIN_NOT_IMPLEMENTED();
}

void ParseCommandLine(CCommandLineInfo& cmdInfo)
{
	MINIWIN_NOT_IMPLEMENTED();
}
