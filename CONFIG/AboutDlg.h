#if !defined(AFX_ABOUTDLG_H)
#define AFX_ABOUTDLG_H

#include "compat.h"
#include "res/resource.h"

#include <QDialog>

namespace Ui
{
class AboutDialog;
}

// VTABLE: CONFIG 0x00406308
// SIZE 0x60
class CAboutDialog : public QDialog {
public:
	CAboutDialog();

protected:
	/*void DoDataExchange(CDataExchange* pDX) override;*/

protected:
	Ui::AboutDialog* m_ui;
};

// SYNTHETIC: CONFIG 0x00403cb0
// CAboutDialog::`scalar deleting destructor'

// FUNCTION: CONFIG 0x00403d30
// CAboutDialog::_GetBaseMessageMap

// FUNCTION: CONFIG 0x00403d40
// CAboutDialog::GetMessageMap

// GLOBAL: CONFIG 0x00406100
// CAboutDialog::messageMap

// GLOBAL: CONFIG 0x00406108
// CAboutDialog::_messageEntries

#endif // !defined(AFX_ABOUTDLG_H)
