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
#include <QMessageBox>
#include <QProcess>
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
	connect(m_ui->fullscreenCheckBox, &QCheckBox::toggled, this, &CMainDialog::OnCheckboxFullscreen);
	connect(m_ui->okButton, &QPushButton::clicked, this, &CMainDialog::accept);
	connect(m_ui->cancelButton, &QPushButton::clicked, this, &CMainDialog::reject);
	connect(m_ui->launchButton, &QPushButton::clicked, this, &CMainDialog::launch);

	connect(m_ui->dataPathOpen, &QPushButton::clicked, this, &CMainDialog::SelectDataPathDialog);
	connect(m_ui->savePathOpen, &QPushButton::clicked, this, &CMainDialog::SelectSavePathDialog);

	connect(m_ui->dataPath, &QLineEdit::editingFinished, this, &CMainDialog::DataPathEdited);
	connect(m_ui->savePath, &QLineEdit::editingFinished, this, &CMainDialog::SavePathEdited);

	connect(m_ui->maxLoDSlider, &QSlider::valueChanged, this, &CMainDialog::MaxLoDChanged);
	connect(m_ui->maxActorsSlider, &QSlider::valueChanged, this, &CMainDialog::MaxActorsChanged);

	layout()->setSizeConstraint(QLayout::SetFixedSize);
}

CMainDialog::~CMainDialog()
{
	delete m_ui;
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

void CMainDialog::launch()
{
	if (m_modified) {
		currentConfigApp->WriteRegisterSettings();
	}

	QDir::setCurrent(QCoreApplication::applicationDirPath());

	QMessageBox msgBox = QMessageBox(
		QMessageBox::Warning,
		QString("Error!"),
		QString("Unable to locate isle executable!"),
		QMessageBox::Close
	);

	if (!QProcess::startDetached("./isle")) {   // Check in isle-config directory
		if (!QProcess::startDetached("isle")) { // Check in $PATH
			msgBox.exec();
		}
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
	m_ui->fullscreenCheckBox->setChecked(currentConfigApp->m_full_screen);
	m_ui->dataPath->setText(QString::fromStdString(currentConfigApp->m_cd_path));
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

void CMainDialog::OnCheckboxFullscreen(bool checked)
{
	currentConfigApp->m_full_screen = checked;
	m_modified = true;
	UpdateInterface();
}

void CMainDialog::SelectDataPathDialog()
{
	QString data_path = QString::fromStdString(currentConfigApp->m_cd_path);
	data_path = QFileDialog::getExistingDirectory(
		this,
		tr("Open Directory"),
		data_path,
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
	);

	QDir data_dir = QDir(data_path);

	if (data_dir.exists()) {
		currentConfigApp->m_cd_path = data_dir.absolutePath().toStdString();
		data_dir.cd(QString("DATA"));
		data_dir.cd(QString("disk"));
		currentConfigApp->m_base_path = data_dir.absolutePath().toStdString();
		m_modified = true;
	}
	UpdateInterface();
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

	QDir save_dir = QDir(save_path);

	if (save_dir.exists()) {
		currentConfigApp->m_save_path = save_dir.absolutePath().toStdString();
		m_modified = true;
	}
	UpdateInterface();
}

void CMainDialog::DataPathEdited()
{
	QDir data_dir = QDir(m_ui->dataPath->text());

	if (data_dir.exists()) {
		currentConfigApp->m_cd_path = data_dir.absolutePath().toStdString();
		data_dir.cd(QString("DATA"));
		data_dir.cd(QString("disk"));
		currentConfigApp->m_base_path = data_dir.absolutePath().toStdString();
		m_modified = true;
	}

	UpdateInterface();
}

void CMainDialog::SavePathEdited()
{

	QDir save_dir = QDir(m_ui->savePath->text());

	if (save_dir.exists()) {
		currentConfigApp->m_save_path = save_dir.absolutePath().toStdString();
		m_modified = true;
	}
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
