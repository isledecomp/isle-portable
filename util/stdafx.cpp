#include "../CONFIG/config.h"
#include "miniwin.h"

BOOL CWinApp::InitInstance()
{
	return TRUE;
}

int CWinApp::ExitInstance()
{
	return 0;
}

CWinApp::~CWinApp() = default;

const char* afxCurrentAppName = "";
AFX_MODULE_STATE g_CustomModuleState;
CWinApp* wndTop;

int main(int argc, char* argv[])
{
	CConfigApp app;
	wndTop = &app;
	if (!app.InitInstance()) {
		return 1;
	}
	return app.ExitInstance();
}
