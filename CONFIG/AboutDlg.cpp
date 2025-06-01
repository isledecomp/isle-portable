#include "AboutDlg.h"

#include "decomp.h"

#include <ui_about.h>

DECOMP_SIZE_ASSERT(CDialog, 0x60)
DECOMP_SIZE_ASSERT(CAboutDialog, 0x60)

// FIXME: disable dialog resizing

// FUNCTION: CONFIG 0x00403c20
CAboutDialog::CAboutDialog() : QDialog()
{
	m_ui = new Ui::AboutDialog;
	m_ui->setupUi(this);
}
