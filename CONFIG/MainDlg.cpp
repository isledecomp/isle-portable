#include "MainDlg.h"

#ifdef MINIWIN
#include "miniwin/windows.h"
#else
#include <windows.h>
#endif

#include "AboutDlg.h"
#include "config.h"
#include "res/resource.h"

#include <QKeyEvent>
#include <mxdirectx/legodxinfo.h>
#include <ui_maindialog.h>

DECOMP_SIZE_ASSERT(CDialog, 0x60)
DECOMP_SIZE_ASSERT(CMainDialog, 0x70)

// FIXME: disable dialog resizing
// FIXME: advanced mode should resize dialog, ignoring advanced controls
// FIXME: list widget should have less rows

// FUNCTION: CONFIG 0x00403d50
CMainDialog::CMainDialog(QWidget* pParent) : QDialog(pParent)
{
	m_ui = new Ui::MainDialog;
	m_ui->setupUi(this);

	// Populate the dialog prior to connecting all signals
	OnInitDialog();

	connect(m_ui->colorPalette16bitRadioButton, &QRadioButton::toggled, this, &CMainDialog::OnRadiobuttonPalette16bit);
	connect(m_ui->colorPalette256RadioButton, &QRadioButton::toggled, this, &CMainDialog::OnRadiobuttonPalette256);
	connect(
		m_ui->modelQualityFastRadioButton,
		&QRadioButton::toggled,
		this,
		&CMainDialog::OnRadiobuttonModelLowQuality
	);
	connect(
		m_ui->modelQualityHighRadioButton,
		&QRadioButton::toggled,
		this,
		&CMainDialog::OnRadiobuttonModelHighQuality
	);
	connect(
		m_ui->textureQualityFastRadioButton,
		&QRadioButton::toggled,
		this,
		&CMainDialog::OnRadiobuttonTextureLowQuality
	);
	connect(
		m_ui->textureQualityHighRadioButton,
		&QRadioButton::toggled,
		this,
		&CMainDialog::OnRadiobuttonTextureHighQuality
	);
	connect(m_ui->devicesList, &QListWidget::currentRowChanged, this, &CMainDialog::OnList3DevicesSelectionChanged);
	connect(m_ui->musicCheckBox, &QCheckBox::toggled, this, &CMainDialog::OnCheckboxMusic);
	connect(m_ui->videomemoryCheckBox, &QCheckBox::toggled, this, &CMainDialog::OnCheckbox3DVideoMemory);
	connect(m_ui->flipVideoMemoryPagesCheckBox, &QCheckBox::toggled, this, &CMainDialog::OnCheckboxFlipVideoMemPages);
	connect(m_ui->sound3DCheckBox, &QCheckBox::toggled, this, &CMainDialog::OnCheckbox3DSound);
	connect(m_ui->joystickCheckBox, &QCheckBox::toggled, this, &CMainDialog::OnCheckboxJoystick);
	connect(m_ui->okButton, &QPushButton::clicked, this, &CMainDialog::accept);
	connect(m_ui->cancelButton, &QPushButton::clicked, this, &CMainDialog::reject);
	connect(m_ui->advancedButton, &QPushButton::clicked, this, &CMainDialog::OnButtonAdvanced);
}
// FUNCTION: CONFIG 0x00403e80
bool CMainDialog::OnInitDialog()
{
	SwitchToAdvanced(false);
	LegoDeviceEnumerate* enumerator = currentConfigApp->m_device_enumerator;
	enumerator->FUN_1009d210();
	m_modified = currentConfigApp->ReadRegisterSettings();
	int driver_i = 0;
	int device_i = 0;
	int selected = 0;
	char device_name[256];
	const list<MxDriver>& driver_list = enumerator->GetDriverList();
	for (list<MxDriver>::const_iterator it_driver = driver_list.begin(); it_driver != driver_list.end(); it_driver++) {
		const MxDriver& driver = *it_driver;
		for (list<Direct3DDeviceInfo>::const_iterator it_device = driver.m_devices.begin();
			 it_device != driver.m_devices.end();
			 it_device++) {
			const Direct3DDeviceInfo& device = *it_device;
			if (&device == currentConfigApp->m_device) {
				selected = device_i;
			}
			device_i += 1;
			sprintf(
				device_name,
				"%s ( %s )",
				device.m_deviceDesc,
				driver_i == 0 ? "Primary Device" : "Secondary Device"
			);
			m_ui->devicesList->addItem(device_name);
		}
		driver_i += 1;
	}
	m_ui->devicesList->setCurrentRow(selected);
	UpdateInterface();
	return true;
}

// FUNCTION: CONFIG 0x00404080
void CMainDialog::keyReleaseEvent(QKeyEvent* event)
{
	if (event->matches(QKeySequence::StandardKey::HelpContents)) {
		CAboutDialog about_dialog;
		about_dialog.exec();
	}
	else {
		QDialog::keyReleaseEvent(event);
	}
}

// FUNCTION: CONFIG 0x00404240
void CMainDialog::OnList3DevicesSelectionChanged(int selected)
{
	LegoDeviceEnumerate* device_enumerator = currentConfigApp->m_device_enumerator;
	device_enumerator->GetDevice(selected, currentConfigApp->m_driver, currentConfigApp->m_device);
	if (currentConfigApp->GetHardwareDeviceColorModel() == D3DCOLOR_NONE) {
		currentConfigApp->m_3d_video_ram = FALSE;
		currentConfigApp->m_flip_surfaces = FALSE;
		m_ui->videomemoryCheckBox->setChecked(currentConfigApp->m_3d_video_ram);
		m_ui->flipVideoMemoryPagesCheckBox->setChecked(currentConfigApp->m_flip_surfaces);
	}
	m_modified = true;
	UpdateInterface();
}

// FUNCTION: CONFIG 0x00404340
void CMainDialog::reject()
{
	QDialog::reject();
}

void CMainDialog::accept()
{
	if (m_modified) {
		currentConfigApp->WriteRegisterSettings();
	}
	QDialog::accept();
}

// FUNCTION: CONFIG 0x00404360
void CMainDialog::UpdateInterface()
{
	currentConfigApp->ValidateSettings();
	m_ui->videomemoryCheckBox->setEnabled(
		!currentConfigApp->m_flip_surfaces && currentConfigApp->GetHardwareDeviceColorModel() == D3DCOLOR_NONE
	);
	m_ui->flipVideoMemoryPagesCheckBox->setChecked(currentConfigApp->m_flip_surfaces);
	m_ui->videomemoryCheckBox->setChecked(currentConfigApp->m_3d_video_ram);
	bool full_screen = currentConfigApp->m_full_screen;
	currentConfigApp->AdjustDisplayBitDepthBasedOnRenderStatus();
	if (full_screen) {
		if (currentConfigApp->m_display_bit_depth == 8) {
			m_ui->colorPalette256RadioButton->setChecked(true);
		}
		else {
			m_ui->colorPalette16bitRadioButton->setChecked(true);
		}
	}
	else {
		m_ui->colorPalette256RadioButton->setChecked(false);
		m_ui->colorPalette256RadioButton->setChecked(false);
		currentConfigApp->m_display_bit_depth = 0;
	}
	m_ui->colorPalette256RadioButton->setEnabled(full_screen && currentConfigApp->GetConditionalDeviceRenderBitDepth());
	m_ui->colorPalette16bitRadioButton->setEnabled(full_screen && currentConfigApp->GetDeviceRenderBitStatus());
	m_ui->sound3DCheckBox->setChecked(currentConfigApp->m_3d_sound);
	switch (currentConfigApp->m_model_quality) {
	case 1:
		m_ui->modelQualityFastRadioButton->setChecked(true);
		break;
	case 2:
		m_ui->modelQualityHighRadioButton->setChecked(true);
		break;
	}
	if (currentConfigApp->m_texture_quality == 0) {
		m_ui->textureQualityFastRadioButton->setChecked(true);
	}
	else {
		m_ui->textureQualityHighRadioButton->setChecked(true);
	}
	m_ui->joystickCheckBox->setChecked(currentConfigApp->m_use_joystick);
	m_ui->musicCheckBox->setChecked(currentConfigApp->m_music);
}

// FUNCTION: CONFIG 0x004045e0
void CMainDialog::OnCheckbox3DSound(bool checked)
{
	currentConfigApp->m_3d_sound = checked;
	m_modified = true;
	UpdateInterface();
}

// FUNCTION: CONFIG 0x00404610
void CMainDialog::OnCheckbox3DVideoMemory(bool checked)
{
	currentConfigApp->m_3d_video_ram = checked;
	m_modified = true;
	UpdateInterface();
}

// FUNCTION: CONFIG 0x00404640
void CMainDialog::OnRadiobuttonPalette16bit(bool checked)
{
	if (checked) {
		currentConfigApp->m_display_bit_depth = 16;
		m_modified = true;
		UpdateInterface();
	}
}

// FUNCTION: CONFIG 0x00404670
void CMainDialog::OnRadiobuttonPalette256(bool checked)
{
	if (checked) {
		currentConfigApp->m_display_bit_depth = 8;
		m_modified = true;
		UpdateInterface();
	}
}

// FUNCTION: CONFIG 0x004046a0
void CMainDialog::OnCheckboxFlipVideoMemPages(bool checked)
{
	currentConfigApp->m_flip_surfaces = checked;
	m_modified = true;
	UpdateInterface();
}

// FUNCTION: CONFIG 0x004046d0
void CMainDialog::OnRadiobuttonModelLowQuality(bool checked)
{
	if (checked) {
		// FIXME: are OnRadiobuttonModelLowQuality and OnRadiobuttonModelHighQuality triggered both?
		qInfo() << "OnRadiobuttonModelLowQuality";
		currentConfigApp->m_model_quality = 1;
		m_modified = true;
		UpdateInterface();
	}
}

// FUNCTION: CONFIG 0x00404700
void CMainDialog::OnRadiobuttonModelHighQuality(bool checked)
{
	if (checked) {
		qInfo() << "OnRadiobuttonModelHighQuality";
		currentConfigApp->m_model_quality = 2;
		m_modified = true;
		UpdateInterface();
	}
}

// FUNCTION: CONFIG 0x00404730
void CMainDialog::OnRadiobuttonTextureLowQuality(bool checked)
{
	if (checked) {
		// FIXME: are OnRadiobuttonTextureLowQuality and OnRadiobuttonTextureHighQuality triggered both?
		qInfo() << "OnRadiobuttonTextureLowQuality";
		currentConfigApp->m_texture_quality = 0;
		m_modified = true;
		UpdateInterface();
	}
}

// FUNCTION: CONFIG 0x00404760
void CMainDialog::OnRadiobuttonTextureHighQuality(bool checked)
{
	if (checked) {
		// FIXME: are OnRadiobuttonTextureLowQuality and OnRadiobuttonTextureHighQuality triggered both?
		qInfo() << "OnRadiobuttonTextureHighQuality";
		currentConfigApp->m_texture_quality = 1;
		m_modified = true;
		UpdateInterface();
	}
}

// FUNCTION: CONFIG 0x00404790
void CMainDialog::OnCheckboxJoystick(bool checked)
{
	currentConfigApp->m_use_joystick = checked;
	m_modified = true;
	UpdateInterface();
}

// FUNCTION: CONFIG 0x004047c0
void CMainDialog::OnButtonAdvanced()
{
	SwitchToAdvanced(!m_advanced);
}

// FUNCTION: CONFIG 0x004047d0
void CMainDialog::SwitchToAdvanced(bool p_advanced)
{
	m_ui->advancedGroup->setVisible(p_advanced);
	layout()->setSizeConstraint(QLayout::SetMinAndMaxSize);
	m_advanced = p_advanced;
}

// FUNCTION: CONFIG 0x004048c0
void CMainDialog::OnCheckboxMusic(bool checked)
{
	currentConfigApp->m_music = checked;
	m_modified = true;
	UpdateInterface();
}
