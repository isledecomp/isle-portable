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

// FUNCTION: CONFIG 0x00403d50
CMainDialog::CMainDialog(QWidget* pParent) : QDialog(pParent)
{
	m_ui = new Ui::MainDialog;
	m_ui->setupUi(this);

	// Populate the dialog prior to connecting all signals
	OnInitDialog();

	connect(m_ui->modelQualityLowRadioButton, &QRadioButton::toggled, this, &CMainDialog::OnRadiobuttonModelLowQuality);
	connect(
		m_ui->modelQualityMediumRadioButton,
		&QRadioButton::toggled,
		this,
		&CMainDialog::OnRadiobuttonModelMediumQuality
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
	connect(m_ui->sound3DCheckBox, &QCheckBox::toggled, this, &CMainDialog::OnCheckbox3DSound);
	connect(m_ui->joystickCheckBox, &QCheckBox::toggled, this, &CMainDialog::OnCheckboxJoystick);
	connect(m_ui->okButton, &QPushButton::clicked, this, &CMainDialog::accept);
	connect(m_ui->cancelButton, &QPushButton::clicked, this, &CMainDialog::reject);

	connect(m_ui->diskPathOpen, &QPushButton::clicked, this, &CMainDialog::SelectDiskPathDialog);
	connect(m_ui->cdPathOpen, &QPushButton::clicked, this, &CMainDialog::SelectCDPathDialog);
	connect(m_ui->mediaPathOpen, &QPushButton::clicked, this, &CMainDialog::SelectMediaPathDialog);
	connect(m_ui->savePathOpen, &QPushButton::clicked, this, &CMainDialog::SelectSavePathDialog);

	connect(m_ui->diskPath, &QLineEdit::textEdited, this, &CMainDialog::DiskPathEdited);
	connect(m_ui->cdPath, &QLineEdit::textEdited, this, &CMainDialog::CDPathEdited);
	connect(m_ui->mediaPath, &QLineEdit::textEdited, this, &CMainDialog::MediaPathEdited);
	connect(m_ui->savePath, &QLineEdit::textEdited, this, &CMainDialog::SavePathEdited);

	connect(m_ui->maxLoDSlider, &QSlider::valueChanged, this, &CMainDialog::MaxLoDChanged);
	connect(m_ui->maxActorsSlider, &QSlider::valueChanged, this, &CMainDialog::MaxActorsChanged);

	layout()->setSizeConstraint(QLayout::SetFixedSize);
}
// FUNCTION: CONFIG 0x00403e80
bool CMainDialog::OnInitDialog()
{
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

	m_ui->maxLoDSlider->setValue((int) currentConfigApp->m_max_lod * 10);
	m_ui->maxActorsSlider->setValue(currentConfigApp->m_max_actors);
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
	bool full_screen = currentConfigApp->m_full_screen;
	currentConfigApp->AdjustDisplayBitDepthBasedOnRenderStatus();
	if (!full_screen) {
		currentConfigApp->m_display_bit_depth = 0;
	}
	m_ui->sound3DCheckBox->setChecked(currentConfigApp->m_3d_sound);
	switch (currentConfigApp->m_model_quality) {
	case 0:
		m_ui->modelQualityLowRadioButton->setChecked(true);
		break;
	case 1:
		m_ui->modelQualityMediumRadioButton->setChecked(true);
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
	m_ui->diskPath->setText(QString::fromStdString(currentConfigApp->m_base_path));
	m_ui->cdPath->setText(QString::fromStdString(currentConfigApp->m_cd_path));
	m_ui->mediaPath->setText(QString::fromStdString(currentConfigApp->m_media_path));
	m_ui->savePath->setText(QString::fromStdString(currentConfigApp->m_save_path));
}

// FUNCTION: CONFIG 0x004045e0
void CMainDialog::OnCheckbox3DSound(bool checked)
{
	currentConfigApp->m_3d_sound = checked;
	m_modified = true;
	UpdateInterface();
}

// FUNCTION: CONFIG 0x004046d0
void CMainDialog::OnRadiobuttonModelLowQuality(bool checked)
{
	if (checked) {
		currentConfigApp->m_model_quality = 0;
		m_modified = true;
		UpdateInterface();
	}
}

void CMainDialog::OnRadiobuttonModelMediumQuality(bool checked)
{
	if (checked) {
		currentConfigApp->m_model_quality = 1;
		m_modified = true;
		UpdateInterface();
	}
}

// FUNCTION: CONFIG 0x00404700
void CMainDialog::OnRadiobuttonModelHighQuality(bool checked)
{
	if (checked) {
		currentConfigApp->m_model_quality = 2;
		m_modified = true;
		UpdateInterface();
	}
}

// FUNCTION: CONFIG 0x00404730
void CMainDialog::OnRadiobuttonTextureLowQuality(bool checked)
{
	if (checked) {
		currentConfigApp->m_texture_quality = 0;
		m_modified = true;
		UpdateInterface();
	}
}

// FUNCTION: CONFIG 0x00404760
void CMainDialog::OnRadiobuttonTextureHighQuality(bool checked)
{
	if (checked) {
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

// FUNCTION: CONFIG 0x004048c0
void CMainDialog::OnCheckboxMusic(bool checked)
{
	currentConfigApp->m_music = checked;
	m_modified = true;
	UpdateInterface();
}

void CMainDialog::SelectDiskPathDialog()
{
	QString disk_path = QString::fromStdString(currentConfigApp->m_base_path);
	disk_path = QFileDialog::getExistingDirectory(
		this,
		tr("Open Directory"),
		disk_path,
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
	);

	if (disk_path.toStdString() != "") {
		currentConfigApp->m_base_path = disk_path.toStdString();
		m_modified = true;
		UpdateInterface();
	}
}

void CMainDialog::SelectCDPathDialog()
{
	QString cd_path = QString::fromStdString(currentConfigApp->m_cd_path);
	cd_path = QFileDialog::getExistingDirectory(
		this,
		tr("Open Directory"),
		cd_path,
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
	);

	if (cd_path.toStdString() != "") {
		currentConfigApp->m_cd_path = cd_path.toStdString();
		m_modified = true;
		UpdateInterface();
	}
}

void CMainDialog::SelectMediaPathDialog()
{
	QString media_path = QString::fromStdString(currentConfigApp->m_media_path);
	media_path = QFileDialog::getExistingDirectory(
		this,
		tr("Open Directory"),
		media_path,
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
	);
	if (media_path.toStdString() != "") {
		currentConfigApp->m_media_path = media_path.toStdString();
		m_modified = true;
		UpdateInterface();
	}
}

void CMainDialog::SelectSavePathDialog()
{
	QString save_path = QString::fromStdString(currentConfigApp->m_save_path);
	save_path = QFileDialog::getExistingDirectory(
		this,
		tr("Open Directory"),
		save_path,
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
	);

	if (save_path.toStdString() != "") {
		currentConfigApp->m_save_path = save_path.toStdString();
		m_modified = true;
		UpdateInterface();
	}
}

void CMainDialog::DiskPathEdited(const QString& text)
{
	currentConfigApp->m_base_path = text.toStdString();
	m_modified = true;
	UpdateInterface();
}

void CMainDialog::CDPathEdited(const QString& text)
{
	currentConfigApp->m_cd_path = text.toStdString();
	m_modified = true;
	UpdateInterface();
}

void CMainDialog::MediaPathEdited(const QString& text)
{
	currentConfigApp->m_media_path = text.toStdString();
	m_modified = true;
	UpdateInterface();
}

void CMainDialog::SavePathEdited(const QString& text)
{
	currentConfigApp->m_save_path = text.toStdString();
	m_modified = true;
	UpdateInterface();
}

void CMainDialog::MaxLoDChanged(int value)
{
	currentConfigApp->m_max_lod = static_cast<float>(value) / 10.0f;
	m_modified = true;
}

void CMainDialog::MaxActorsChanged(int value)
{
	currentConfigApp->m_max_actors = value;
	m_modified = true;
}
