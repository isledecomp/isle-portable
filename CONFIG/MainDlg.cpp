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
#include <SDL3/SDL.h>
#include <cmath>
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

	connect(m_ui->windowedRadioButton, &QRadioButton::toggled, this, &CMainDialog::OnRadioWindowed);
	connect(m_ui->fullscreenRadioButton, &QRadioButton::toggled, this, &CMainDialog::OnRadioFullscreen);
	connect(m_ui->exFullscreenRadioButton, &QRadioButton::toggled, this, &CMainDialog::OnRadioExclusiveFullscreen);
	connect(m_ui->devicesList, &QListWidget::currentRowChanged, this, &CMainDialog::OnList3DevicesSelectionChanged);
	connect(m_ui->musicCheckBox, &QCheckBox::toggled, this, &CMainDialog::OnCheckboxMusic);
	connect(m_ui->sound3DCheckBox, &QCheckBox::toggled, this, &CMainDialog::OnCheckbox3DSound);
	connect(m_ui->rumbleCheckBox, &QCheckBox::toggled, this, &CMainDialog::OnCheckboxRumble);
	connect(m_ui->textureCheckBox, &QCheckBox::toggled, this, &CMainDialog::OnCheckboxTexture);
	connect(m_ui->customAssetsCheckBox, &QCheckBox::toggled, this, &CMainDialog::OnCheckboxCustomAssets);
	connect(m_ui->touchComboBox, &QComboBox::currentIndexChanged, this, &CMainDialog::TouchControlsChanged);
	connect(m_ui->transitionTypeComboBox, &QComboBox::currentIndexChanged, this, &CMainDialog::TransitionTypeChanged);
	connect(m_ui->exFullResComboBox, &QComboBox::currentIndexChanged, this, &CMainDialog::ExclusiveResolutionChanged);
	connect(m_ui->okButton, &QPushButton::clicked, this, &CMainDialog::accept);
	connect(m_ui->cancelButton, &QPushButton::clicked, this, &CMainDialog::reject);
	connect(m_ui->launchButton, &QPushButton::clicked, this, &CMainDialog::launch);

	connect(m_ui->dataPathOpen, &QPushButton::clicked, this, &CMainDialog::SelectDataPathDialog);
	connect(m_ui->savePathOpen, &QPushButton::clicked, this, &CMainDialog::SelectSavePathDialog);

	connect(m_ui->dataPath, &QLineEdit::editingFinished, this, &CMainDialog::DataPathEdited);
	connect(m_ui->savePath, &QLineEdit::editingFinished, this, &CMainDialog::SavePathEdited);

	connect(m_ui->texturePathOpen, &QPushButton::clicked, this, &CMainDialog::SelectTexturePathDialog);
	connect(m_ui->texturePath, &QLineEdit::editingFinished, this, &CMainDialog::TexturePathEdited);

	connect(m_ui->addCustomAssetPath, &QPushButton::clicked, this, &CMainDialog::AddCustomAssetPath);
	connect(m_ui->removeCustomAssetPath, &QPushButton::clicked, this, &CMainDialog::RemoveCustomAssetPath);
	connect(m_ui->customAssetPaths, &QListWidget::currentRowChanged, this, &CMainDialog::SelectedPathChanged);
	connect(m_ui->customAssetPaths, &QListWidget::itemActivated, this, &CMainDialog::EditCustomAssetPath);

	connect(m_ui->maxLoDSlider, &QSlider::valueChanged, this, &CMainDialog::MaxLoDChanged);
	connect(m_ui->maxLoDSlider, &QSlider::sliderMoved, this, &CMainDialog::MaxLoDChanged);
	connect(m_ui->maxActorsSlider, &QSlider::valueChanged, this, &CMainDialog::MaxActorsChanged);
	connect(m_ui->maxActorsSlider, &QSlider::sliderMoved, this, &CMainDialog::MaxActorsChanged);

	connect(m_ui->msaaSlider, &QSlider::valueChanged, this, &CMainDialog::MSAAChanged);
	connect(m_ui->msaaSlider, &QSlider::sliderMoved, this, &CMainDialog::MSAAChanged);
	connect(m_ui->AFSlider, &QSlider::valueChanged, this, &CMainDialog::AFChanged);
	connect(m_ui->AFSlider, &QSlider::sliderMoved, this, &CMainDialog::AFChanged);

	connect(m_ui->aspectRatioComboBox, &QComboBox::currentIndexChanged, this, &CMainDialog::AspectRatioChanged);
	connect(m_ui->xResSpinBox, &QSpinBox::valueChanged, this, &CMainDialog::XResChanged);
	connect(m_ui->yResSpinBox, &QSpinBox::valueChanged, this, &CMainDialog::YResChanged);
	connect(m_ui->framerateSpinBox, &QSpinBox::valueChanged, this, &CMainDialog::FramerateChanged);

	if (currentConfigApp->m_ram_quality_limit != 0) {
		m_modified = true;
		const QString ramError = QString("Insufficient RAM!");
		m_ui->sound3DCheckBox->setChecked(false);
		m_ui->sound3DCheckBox->setEnabled(false);
		m_ui->sound3DCheckBox->setToolTip(ramError);
		m_ui->modelQualityHighRadioButton->setEnabled(false);
		m_ui->modelQualityHighRadioButton->setToolTip(ramError);
		m_ui->modelQualityLowRadioButton->setEnabled(true);
		if (currentConfigApp->m_ram_quality_limit == 2) {
			m_ui->modelQualityLowRadioButton->setChecked(true);
			m_ui->modelQualityMediumRadioButton->setEnabled(false);
			m_ui->modelQualityMediumRadioButton->setToolTip(ramError);
			m_ui->maxLoDSlider->setMaximum(30);
			m_ui->maxActorsSlider->setMaximum(15);
		}
		else {
			m_ui->modelQualityMediumRadioButton->setChecked(true);
			m_ui->modelQualityMediumRadioButton->setEnabled(true);
			m_ui->maxLoDSlider->setMaximum(40);
			m_ui->maxActorsSlider->setMaximum(30);
		}
	}
	else {
		m_ui->sound3DCheckBox->setEnabled(true);
		m_ui->modelQualityLowRadioButton->setEnabled(true);
		m_ui->modelQualityMediumRadioButton->setEnabled(true);
		m_ui->modelQualityHighRadioButton->setEnabled(true);
		m_ui->maxLoDSlider->setMaximum(60);
		m_ui->maxActorsSlider->setMaximum(40);
	}
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
	const list<MxDriver>& driver_list = enumerator->m_ddInfo;
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
	m_ui->LoDNum->setNum((int) currentConfigApp->m_max_lod * 10);
	m_ui->maxActorsSlider->setValue(currentConfigApp->m_max_actors);
	m_ui->maxActorsNum->setNum(currentConfigApp->m_max_actors);

	m_ui->exFullResComboBox->clear();

	int displayModeCount;
	displayModes = SDL_GetFullscreenDisplayModes(SDL_GetPrimaryDisplay(), &displayModeCount);

	for (int i = 0; i < displayModeCount; ++i) {
		QString mode =
			QString("%1x%2 @ %3Hz").arg(displayModes[i]->w).arg(displayModes[i]->h).arg(displayModes[i]->refresh_rate);
		m_ui->exFullResComboBox->addItem(mode);

		if ((displayModes[i]->w == currentConfigApp->m_exf_x_res) &&
			(displayModes[i]->h == currentConfigApp->m_exf_y_res) &&
			(displayModes[i]->refresh_rate == currentConfigApp->m_exf_fps)) {
			m_ui->exFullResComboBox->setCurrentIndex(i);
		}
	}
	SDL_free(displayModes);

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
	if (currentConfigApp->m_exclusive_full_screen) {
		m_ui->exFullscreenRadioButton->setChecked(true);
		m_ui->resolutionBox->setEnabled(false);
		m_ui->exFullResContainer->setEnabled(true);
	}
	else {
		m_ui->resolutionBox->setEnabled(true);
		m_ui->exFullResContainer->setEnabled(false);
		if (currentConfigApp->m_full_screen) {
			m_ui->fullscreenRadioButton->setChecked(true);
		}
		else {
			m_ui->windowedRadioButton->setChecked(true);
		}
	}
	m_ui->musicCheckBox->setChecked(currentConfigApp->m_music);
	m_ui->rumbleCheckBox->setChecked(currentConfigApp->m_haptic);
	m_ui->touchComboBox->setCurrentIndex(currentConfigApp->m_touch_scheme);
	m_ui->transitionTypeComboBox->setCurrentIndex(currentConfigApp->m_transition_type);
	m_ui->dataPath->setText(QString::fromStdString(currentConfigApp->m_cd_path));
	m_ui->savePath->setText(QString::fromStdString(currentConfigApp->m_save_path));

	m_ui->textureCheckBox->setChecked(currentConfigApp->m_texture_load);
	QString texture_path = QString::fromStdString(currentConfigApp->m_texture_path);
	if (texture_path.startsWith(QDir::separator())) {
		texture_path.remove(0, 1);
	}
	m_ui->texturePath->setText(texture_path);
	m_ui->texturePath->setEnabled(currentConfigApp->m_texture_load);
	m_ui->texturePathOpen->setEnabled(currentConfigApp->m_texture_load);

	m_ui->customAssetsCheckBox->setChecked(currentConfigApp->m_custom_assets_enabled);
	m_ui->customAssetPathContainer->setEnabled(currentConfigApp->m_custom_assets_enabled);
	m_ui->customAssetPaths->setEnabled(currentConfigApp->m_custom_assets_enabled);
	m_ui->addCustomAssetPath->setEnabled(currentConfigApp->m_custom_assets_enabled);
	m_ui->removeCustomAssetPath->setEnabled(false);

	m_ui->customAssetPaths->clear();
	assetPaths = QString::fromStdString(currentConfigApp->m_custom_asset_path).split(u',');
	for (QString& path : assetPaths) {
		if (path.startsWith(QDir::separator())) {
			path.remove(0, 1);
		}
	}
	m_ui->customAssetPaths->addItems(assetPaths);

	m_ui->aspectRatioComboBox->setCurrentIndex(currentConfigApp->m_aspect_ratio);
	m_ui->xResSpinBox->setValue(currentConfigApp->m_x_res);
	m_ui->yResSpinBox->setValue(currentConfigApp->m_y_res);
	m_ui->framerateSpinBox->setValue(static_cast<int>(std::round(1000.0f / currentConfigApp->m_frame_delta)));

	m_ui->maxLoDSlider->setValue((int) (currentConfigApp->m_max_lod * 10));
	m_ui->LoDNum->setNum(currentConfigApp->m_max_lod);
	m_ui->maxActorsSlider->setValue(currentConfigApp->m_max_actors);
	m_ui->maxActorsNum->setNum(currentConfigApp->m_max_actors);
	m_ui->msaaSlider->setValue(log2(currentConfigApp->m_msaa));
	m_ui->msaaNum->setNum(currentConfigApp->m_msaa);
	m_ui->AFSlider->setValue(log2(currentConfigApp->m_anisotropy));
	m_ui->AFNum->setNum(currentConfigApp->m_anisotropy);
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

void CMainDialog::OnRadioWindowed(bool checked)
{
	if (checked) {
		currentConfigApp->m_full_screen = false;
		currentConfigApp->m_exclusive_full_screen = false;
		m_ui->resolutionBox->setEnabled(true);
		m_ui->exFullResContainer->setEnabled(false);
		m_modified = true;
		UpdateInterface();
	}
}

void CMainDialog::OnRadioFullscreen(bool checked)
{
	if (checked) {
		currentConfigApp->m_full_screen = true;
		currentConfigApp->m_exclusive_full_screen = false;
		m_ui->resolutionBox->setEnabled(true);
		m_ui->exFullResContainer->setEnabled(false);
		m_modified = true;
		UpdateInterface();
	}
}

void CMainDialog::OnRadioExclusiveFullscreen(bool checked)
{
	if (checked) {
		currentConfigApp->m_full_screen = true;
		currentConfigApp->m_exclusive_full_screen = true;
		m_ui->resolutionBox->setEnabled(false);
		m_ui->exFullResContainer->setEnabled(true);
		m_modified = true;
		UpdateInterface();
	}
}

// FUNCTION: CONFIG 0x004048c0
void CMainDialog::OnCheckboxMusic(bool checked)
{
	currentConfigApp->m_music = checked;
	m_modified = true;
	UpdateInterface();
}

void CMainDialog::OnCheckboxRumble(bool checked)
{
	currentConfigApp->m_haptic = checked;
	m_modified = true;
	UpdateInterface();
}

void CMainDialog::OnCheckboxTexture(bool checked)
{
	currentConfigApp->m_texture_load = checked;
	m_modified = true;
	UpdateInterface();
}

void CMainDialog::OnCheckboxCustomAssets(bool checked)
{
	currentConfigApp->m_custom_assets_enabled = checked;
	m_modified = true;
	UpdateInterface();
}

void CMainDialog::TouchControlsChanged(int index)
{
	currentConfigApp->m_touch_scheme = index;
	m_modified = true;
	UpdateInterface();
}

void CMainDialog::TransitionTypeChanged(int index)
{
	currentConfigApp->m_transition_type = index;
	m_modified = true;
	UpdateInterface();
}

void CMainDialog::ExclusiveResolutionChanged(int index)
{
	currentConfigApp->m_exf_x_res = displayModes[index]->w;
	currentConfigApp->m_exf_y_res = displayModes[index]->h;
	currentConfigApp->m_exf_fps = displayModes[index]->refresh_rate;
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

	if (!data_path.isEmpty()) {
		QDir data_dir = QDir(data_path);
		if (data_dir.exists()) {
			currentConfigApp->m_cd_path = data_dir.absolutePath().toStdString();
			data_dir.cd(QString("DATA"));
			data_dir.cd(QString("disk"));
			currentConfigApp->m_base_path = data_dir.absolutePath().toStdString();
			m_modified = true;
		}
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

	if (!save_path.isEmpty()) {
		QDir save_dir = QDir(save_path);
		if (save_dir.exists()) {
			currentConfigApp->m_save_path = save_dir.absolutePath().toStdString();
			m_modified = true;
		}
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
	UpdateInterface();
}

void CMainDialog::MaxActorsChanged(int value)
{
	currentConfigApp->m_max_actors = value;
	m_modified = true;
	UpdateInterface();
}

void CMainDialog::MSAAChanged(int value)
{
	currentConfigApp->m_msaa = exp2(value);
	m_modified = true;
	UpdateInterface();
}

void CMainDialog::AFChanged(int value)
{
	currentConfigApp->m_anisotropy = exp2(value);
	m_modified = true;
	UpdateInterface();
}

void CMainDialog::SelectTexturePathDialog()
{
	QDir data_path = QDir(QString::fromStdString(currentConfigApp->m_cd_path));
	QString texture_path = QString::fromStdString(currentConfigApp->m_texture_path);
	if (texture_path.startsWith(QDir::separator())) {
		texture_path.remove(0, 1);
	}
	texture_path = data_path.absoluteFilePath(texture_path);
	texture_path = QFileDialog::getExistingDirectory(
		this,
		tr("Open Directory"),
		texture_path,
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
	);

	if (!texture_path.isEmpty() && data_path.exists(texture_path)) {
		texture_path = data_path.relativeFilePath(texture_path);
		texture_path.prepend(QDir::separator());
		currentConfigApp->m_texture_path = texture_path.toStdString();
		m_modified = true;
	}
	UpdateInterface();
}

void CMainDialog::TexturePathEdited()
{
	QString texture_path = m_ui->texturePath->text();
	QDir data_path = QDir(QString::fromStdString(currentConfigApp->m_cd_path));

	if (texture_path.startsWith(QDir::separator())) {
		texture_path.remove(0, 1);
	}
	if (data_path.exists(data_path.absoluteFilePath(texture_path))) {
		texture_path = data_path.relativeFilePath(texture_path);
		texture_path.prepend(QDir::separator());
		currentConfigApp->m_texture_path = texture_path.toStdString();
		m_modified = true;
	}
	UpdateInterface();
}

void CMainDialog::AddCustomAssetPath()
{
	QDir data_path = QDir(QString::fromStdString(currentConfigApp->m_cd_path));
	QStringList new_files = QFileDialog::getOpenFileNames(
		this,
		"Select one or more files to open",
		data_path.absolutePath(),
		"Interleaf files (*.si)"
	);
	if (!new_files.isEmpty()) {
		for (QString& item : new_files) {
			item = data_path.relativeFilePath(item);
		}
		assetPaths += new_files;
		m_modified = true;
	}
	UpdateAssetPaths();
}

void CMainDialog::RemoveCustomAssetPath()
{
	assetPaths.removeAt(m_ui->customAssetPaths->currentRow());
	m_modified = true;
	UpdateAssetPaths();
}

void CMainDialog::SelectedPathChanged(int currentRow)
{
	m_ui->removeCustomAssetPath->setEnabled(currentRow != -1 ? true : false);
}

void CMainDialog::EditCustomAssetPath()
{
	QDir data_path = QDir(QString::fromStdString(currentConfigApp->m_cd_path));
	QString prev_asset_path = assetPaths[m_ui->customAssetPaths->currentRow()];
	QString new_file = QFileDialog::getOpenFileName(
		this,
		"Open File",
		data_path.absoluteFilePath(prev_asset_path),
		"Interleaf files (*.si)"
	);
	if (!new_file.isEmpty()) {
		new_file = data_path.relativeFilePath(new_file);
		assetPaths[m_ui->customAssetPaths->currentRow()] = new_file;
		m_modified = true;
	}
	UpdateAssetPaths();
}

void CMainDialog::UpdateAssetPaths()
{
	assetPaths.removeDuplicates();

	for (QString& path : assetPaths) {
		if (!path.startsWith(QDir::separator())) {
			path.prepend(QDir::separator());
		}
	}
	currentConfigApp->m_custom_asset_path = assetPaths.join(u',').toStdString();
	UpdateInterface();
}

void CMainDialog::AspectRatioChanged(int index)
{
	currentConfigApp->m_aspect_ratio = index;
	EnsureAspectRatio();
	m_modified = true;
	UpdateInterface();
}

void CMainDialog::XResChanged(int i)
{
	currentConfigApp->m_x_res = i;
	m_modified = true;
	UpdateInterface();
}

void CMainDialog::YResChanged(int i)
{
	currentConfigApp->m_y_res = i;
	EnsureAspectRatio();
	m_modified = true;
	UpdateInterface();
}

void CMainDialog::EnsureAspectRatio()
{
	if (currentConfigApp->m_aspect_ratio != 3) {
		m_ui->xResSpinBox->setReadOnly(true);
		switch (currentConfigApp->m_aspect_ratio) {
		case 0: {
			float standardAspect = 4.0f / 3.0f;
			currentConfigApp->m_x_res = static_cast<int>(std::round((currentConfigApp->m_y_res) * standardAspect));
			break;
		}
		case 1: {
			float wideAspect = 16.0f / 9.0f;
			currentConfigApp->m_x_res = static_cast<int>(std::round((currentConfigApp->m_y_res) * wideAspect));
			break;
		}
		case 2: {
			currentConfigApp->m_x_res = currentConfigApp->m_y_res;
			break;
		}
		}
	}
	else {
		m_ui->xResSpinBox->setReadOnly(false);
	}
}

void CMainDialog::FramerateChanged(int i)
{
	currentConfigApp->m_frame_delta = (1000.0f / static_cast<float>(i));
	m_modified = true;
	UpdateInterface();
}
