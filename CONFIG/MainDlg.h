#if !defined(AFX_MAINDLG_H)
#define AFX_MAINDLG_H

#include "compat.h"
#include "decomp.h"
#include "res/resource.h"

#include <QDialog>
#include <QFileDialog>

namespace Ui
{
class MainDialog;
}

// VTABLE: CONFIG 0x004063e0
// SIZE 0x70
class CMainDialog : public QDialog {
	Q_OBJECT

public:
	CMainDialog(QWidget* pParent = nullptr);
	~CMainDialog() override;

protected:
	void UpdateInterface();

private:
	bool m_modified = false;
	bool m_advanced = false;
	Ui::MainDialog* m_ui = nullptr;

	void keyReleaseEvent(QKeyEvent* event) override;
	bool OnInitDialog();
private slots:
	void OnList3DevicesSelectionChanged(int row);
	void OnCheckbox3DSound(bool checked);
	void OnRadiobuttonModelLowQuality(bool checked);
	void OnRadiobuttonModelMediumQuality(bool checked);
	void OnRadiobuttonModelHighQuality(bool checked);
	void OnRadiobuttonTextureLowQuality(bool checked);
	void OnRadiobuttonTextureHighQuality(bool checked);
	void OnCheckboxJoystick(bool checked);
	void OnCheckboxMusic(bool checked);
	void OnCheckboxFullscreen(bool checked);
	void accept() override;
	void reject() override;
	void launch();
	void SelectDataPathDialog();
	void SelectSavePathDialog();
	void DataPathEdited();
	void SavePathEdited();
	void MaxLoDChanged(int value);
	void MaxActorsChanged(int value);
};

// SYNTHETIC: CONFIG 0x00403de0
// CMainDialog::`scalar deleting destructor'

// FUNCTION: CONFIG 0x00403e60
// CMainDialog::_GetBaseMessageMap

// FUNCTION: CONFIG 0x00403e70
// CMainDialog::GetMessageMap

// GLOBAL: CONFIG 0x00406120
// CMainDialog::messageMap

// GLOBAL: CONFIG 0x00406128
// CMainDialog::_messageEntries

#endif // !defined(AFX_MAINDLG_H)
