#include "config.h"

#include "MainDlg.h"
#include "detectdx5.h"

#include <mxdirectx/legodxinfo.h>
#include <mxdirectx/mxdirect3d.h>
#ifdef MINIWIN
#include "miniwin/direct.h"
#include "miniwin/process.h"
#else
#include <direct.h>  // _chdir
#include <process.h> // _spawnl
#endif

#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <SDL3/SDL.h>
#include <iniparser.h>

DECOMP_SIZE_ASSERT(CWinApp, 0xc4)
DECOMP_SIZE_ASSERT(CConfigApp, 0x108)

DECOMP_STATIC_ASSERT(offsetof(CConfigApp, m_display_bit_depth) == 0xd0)

// FUNCTION: CONFIG 0x00402c40
CConfigApp::CConfigApp()
{
	char* prefPath = SDL_GetPrefPath("isledecomp", "isle");
	char* iniConfig;
	if (prefPath) {
		m_iniPath = std::string{prefPath} + "isle.ini";
	}
	else {
		m_iniPath = "isle.ini";
	}
	SDL_free(prefPath);
}

// FUNCTION: CONFIG 0x00402dc0
bool CConfigApp::InitInstance()
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		QString err = QString{"SDL failed to initialize ("} + SDL_GetError() + ")";
		QMessageBox::warning(nullptr, "SDL initialization error", err);
		return false;
	}
	if (!DetectDirectX5()) {
		QMessageBox::warning(
			nullptr,
			"Missing DirectX",
			"\"LEGO\xc2\xae Island\" is not detecting DirectX 5 or later.  Please quit all other applications and try "
			"again."
		);
		return false;
	}
	m_device_enumerator = new LegoDeviceEnumerate;
	if (m_device_enumerator->DoEnumerate()) {
		return FALSE;
	}
	m_driver = NULL;
	m_device = NULL;
	m_full_screen = TRUE;
	m_wide_view_angle = TRUE;
	m_use_joystick = TRUE;
	m_music = TRUE;
	m_flip_surfaces = FALSE;
	m_3d_video_ram = FALSE;
	m_joystick_index = -1;
	m_display_bit_depth = 16;
	int totalRamMiB = SDL_GetSystemRAM();
	if (totalRamMiB < 12) {
		m_3d_sound = FALSE;
		m_model_quality = 0;
		m_texture_quality = 1;
		m_max_lod = 1.5f;
		m_max_actors = 5;
	}
	else if (totalRamMiB < 20) {
		m_3d_sound = FALSE;
		m_model_quality = 1;
		m_texture_quality = 1;
		m_max_lod = 2.5f;
		m_max_actors = 10;
	}
	else {
		m_model_quality = 2;
		m_3d_sound = TRUE;
		m_texture_quality = 1;
		m_max_lod = 3.5f;
		m_max_actors = 20;
	}
	return true;
}

// FUNCTION: CONFIG 0x004033d0
bool CConfigApp::IsDeviceInBasicRGBMode() const
{
	/*
	 * BUG: should be:
	 *  return !GetHardwareDeviceColorModel() && (m_device->m_HELDesc.dcmColorModel & D3DCOLOR_RGB);
	 */
	return GetHardwareDeviceColorModel() == D3DCOLOR_NONE && m_device->m_HELDesc.dcmColorModel == D3DCOLOR_RGB;
}

// FUNCTION: CONFIG 0x00403400
D3DCOLORMODEL CConfigApp::GetHardwareDeviceColorModel() const
{
	return m_device->m_HWDesc.dcmColorModel;
}

// FUNCTION: CONFIG 0x00403410
bool CConfigApp::IsPrimaryDriver() const
{
	return m_driver == &m_device_enumerator->GetDriverList().front();
}

// FUNCTION: CONFIG 0x00403430
bool CConfigApp::ReadRegisterSettings()
{
	int tmp = -1;
#define NOT_FOUND (-2)

	dictionary* dict = iniparser_load(m_iniPath.c_str());
	if (!dict) {
		dict = dictionary_new(0);
	}

	const char* device3D = iniparser_getstring(dict, "isle:3D Device ID", nullptr);
	if (device3D) {
		tmp = m_device_enumerator->ParseDeviceName(device3D);
		if (tmp >= 0) {
			tmp = m_device_enumerator->GetDevice(tmp, m_driver, m_device);
		}
	}
	if (tmp != 0) {
		m_device_enumerator->FUN_1009d210();
		tmp = m_device_enumerator->GetBestDevice();
		m_device_enumerator->GetDevice(tmp, m_driver, m_device);
	}
	m_base_path = iniparser_getstring(dict, "isle:diskpath", m_base_path.c_str());
	m_cd_path = iniparser_getstring(dict, "isle:cdpath", m_cd_path.c_str());
	m_media_path = iniparser_getstring(dict, "isle:mediapath", m_media_path.c_str());
	m_save_path = iniparser_getstring(dict, "isle:savepath", m_save_path.c_str());
	m_display_bit_depth = iniparser_getint(dict, "isle:Display Bit Depth", -1);
	m_flip_surfaces = iniparser_getboolean(dict, "isle:Flip Surfaces", m_flip_surfaces);
	m_full_screen = iniparser_getboolean(dict, "isle:Full Screen", m_full_screen);
	m_3d_video_ram = iniparser_getboolean(dict, "isle:Back Buffers in Video RAM", m_3d_video_ram);
	m_wide_view_angle = iniparser_getboolean(dict, "isle:Wide View Angle", m_wide_view_angle);
	m_3d_sound = iniparser_getboolean(dict, "isle:3DSound", m_3d_sound);
	m_draw_cursor = iniparser_getboolean(dict, "isle:Draw Cursor", m_draw_cursor);
	m_model_quality = iniparser_getint(dict, "isle:Island Quality", m_model_quality);
	m_texture_quality = iniparser_getint(dict, "isle:Island Texture", m_texture_quality);
	m_use_joystick = iniparser_getboolean(dict, "isle:UseJoystick", m_use_joystick);
	m_music = iniparser_getboolean(dict, "isle:Music", m_music);
	m_joystick_index = iniparser_getint(dict, "isle:JoystickIndex", m_joystick_index);
	m_max_lod = iniparser_getdouble(dict, "isle:Max LOD", m_max_lod);
	m_max_actors = iniparser_getint(dict, "isle:Max Allowed Extras", m_max_actors);
	return true;
}

// FUNCTION: CONFIG 0x00403630
bool CConfigApp::ValidateSettings()
{
	BOOL is_modified = FALSE;

	if (!IsPrimaryDriver() && !m_full_screen) {
		m_full_screen = TRUE;
		is_modified = TRUE;
	}
	if (IsDeviceInBasicRGBMode()) {
		if (m_3d_video_ram) {
			m_3d_video_ram = FALSE;
			is_modified = TRUE;
		}
		if (m_flip_surfaces) {
			m_flip_surfaces = FALSE;
			is_modified = TRUE;
		}
		if (m_display_bit_depth != 16) {
			m_display_bit_depth = 16;
			is_modified = TRUE;
		}
	}
	if (GetHardwareDeviceColorModel() == D3DCOLOR_NONE) {
		m_draw_cursor = FALSE;
		is_modified = TRUE;
	}
	else {
		if (!m_3d_video_ram) {
			m_3d_video_ram = TRUE;
			is_modified = TRUE;
		}
		if (m_full_screen && !m_flip_surfaces) {
			m_flip_surfaces = TRUE;
			is_modified = TRUE;
		}
	}
	if (m_flip_surfaces) {
		if (!m_3d_video_ram) {
			m_3d_video_ram = TRUE;
			is_modified = TRUE;
		}
		if (!m_full_screen) {
			m_full_screen = TRUE;
			is_modified = TRUE;
		}
	}
	if ((m_display_bit_depth != 8 && m_display_bit_depth != 16) && (m_display_bit_depth != 0 || m_full_screen)) {
		m_display_bit_depth = 16;
		is_modified = TRUE;
	}
	if (m_model_quality < 0 || m_model_quality > 2) {
		m_model_quality = 1;
		is_modified = TRUE;
	}
	if (m_texture_quality < 0 || m_texture_quality > 1) {
		m_texture_quality = 0;
		is_modified = TRUE;
	}

	if (m_max_lod < 0.0f || m_max_lod > 5.0f) {
		m_max_lod = 3.5f;
		is_modified = TRUE;
	}
	if (m_max_actors < 5 || m_max_actors > 40) {
		m_max_actors = 20;
		is_modified = TRUE;
	}

	return is_modified;
}

// FUNCTION: CONFIG 0x004037a0
DWORD CConfigApp::GetConditionalDeviceRenderBitDepth() const
{
	if (IsDeviceInBasicRGBMode()) {
		return 0;
	}
	if (GetHardwareDeviceColorModel() != D3DCOLOR_NONE) {
		return 0;
	}
	return (m_device->m_HELDesc.dwDeviceRenderBitDepth & DDBD_8) == DDBD_8;
}

// FUNCTION: CONFIG 0x004037e0
DWORD CConfigApp::GetDeviceRenderBitStatus() const
{
	if (GetHardwareDeviceColorModel() != D3DCOLOR_NONE) {
		return (m_device->m_HWDesc.dwDeviceRenderBitDepth & DDBD_16) == DDBD_16;
	}
	else {
		return (m_device->m_HELDesc.dwDeviceRenderBitDepth & DDBD_16) == DDBD_16;
	}
}

// FUNCTION: CONFIG 0x00403810
bool CConfigApp::AdjustDisplayBitDepthBasedOnRenderStatus()
{
	if (m_display_bit_depth == 8) {
		if (GetConditionalDeviceRenderBitDepth()) {
			return FALSE;
		}
	}
	if (m_display_bit_depth == 16) {
		if (GetDeviceRenderBitStatus()) {
			return FALSE;
		}
	}
	if (GetConditionalDeviceRenderBitDepth()) {
		m_display_bit_depth = 8;
		return TRUE;
	}
	if (GetDeviceRenderBitStatus()) {
		m_display_bit_depth = 16;
		return TRUE;
	}
	m_display_bit_depth = 16;
	return TRUE;
}

// FUNCTION: CONFIG 00403890
void CConfigApp::WriteRegisterSettings() const

{
	char buffer[128];
#define SetIniBool(DICT, NAME, VALUE) iniparser_set(DICT, NAME, VALUE ? "true" : "false")
#define SetIniInt(DICT, NAME, VALUE)                                                                                   \
	do {                                                                                                               \
		sprintf(buffer, "%d", VALUE);                                                                                  \
		iniparser_set(DICT, NAME, buffer);                                                                             \
	} while (0)

	m_device_enumerator->FormatDeviceName(buffer, m_driver, m_device);

	dictionary* dict = dictionary_new(0);
	iniparser_set(dict, "isle", NULL);
	if (m_device_enumerator->FormatDeviceName(buffer, m_driver, m_device) >= 0) {
		iniparser_set(dict, "isle:3D Device ID", buffer);
	}
	iniparser_set(dict, "isle:diskpath", m_base_path.c_str());
	iniparser_set(dict, "isle:cdpath", m_cd_path.c_str());
	iniparser_set(dict, "isle:mediapath", m_media_path.c_str());
	iniparser_set(dict, "isle:savepath", m_save_path.c_str());

	SetIniInt(dict, "isle:Display Bit Depth", m_display_bit_depth);
	SetIniBool(dict, "isle:Flip Surfaces", m_flip_surfaces);
	SetIniBool(dict, "isle:Full Screen", m_full_screen);
	SetIniBool(dict, "isle:Wide View Angle", m_wide_view_angle);

	SetIniBool(dict, "isle:3DSound", m_3d_sound);
	SetIniBool(dict, "isle:Music", m_music);

	SetIniBool(dict, "isle:UseJoystick", m_use_joystick);
	SetIniInt(dict, "isle:JoystickIndex", m_joystick_index);
	SetIniBool(dict, "isle:Draw Cursor", m_draw_cursor);

	SetIniBool(dict, "isle:Back Buffers in Video RAM", m_3d_video_ram);

	SetIniInt(dict, "isle:Island Quality", m_model_quality);
	SetIniInt(dict, "isle:Island Texture", m_texture_quality);

	iniparser_set(dict, "isle:Max LOD", std::to_string(m_max_lod).c_str());
	SetIniInt(dict, "isle:Max Allowed Extras", m_max_actors);

#undef SetIniBool
#undef SetIniInt

	FILE* iniFP = fopen(m_iniPath.c_str(), "wb");
	if (iniFP) {
		iniparser_dump_ini(dict, iniFP);
		qInfo() << "New config written at" << QString::fromStdString(m_iniPath);
		fclose(iniFP);
	}
	else {
		QMessageBox::warning(nullptr, "Failed to save ini", "Failed to save ini");
	}
	iniparser_freedict(dict);
}

// FUNCTION: CONFIG 0x00403a90
int CConfigApp::ExitInstance()
{
	if (m_device_enumerator) {
		delete m_device_enumerator;
		m_device_enumerator = NULL;
	}
	SDL_Quit();
	return 0;
}

void CConfigApp::SetIniPath(const std::string& p_path)
{
	m_iniPath = p_path;
}
const std::string& CConfigApp::GetIniPath() const
{
	return m_iniPath;
}

// GLOBAL: CONFIG 0x00408e50
CConfigApp g_theApp;

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	QCoreApplication::setApplicationName("config");
	QCoreApplication::setApplicationVersion("1.0");

	QCommandLineParser parser;
	parser.setApplicationDescription("Configure LEGO Island");
	parser.addHelpOption();
	parser.addVersionOption();

	QCommandLineOption iniOption(
		QStringList() << "ini",
		QCoreApplication::translate("config", "Set INI path."),
		QCoreApplication::translate("config", "path")
	);
	parser.addOption(iniOption);
	parser.process(app);

	if (parser.isSet(iniOption)) {
		g_theApp.SetIniPath(parser.value(iniOption).toStdString());
	}
	qInfo() << "Ini path =" << QString::fromStdString(g_theApp.GetIniPath());

	int result = 1;
	if (g_theApp.InitInstance()) {
		CMainDialog main_dialog;
		main_dialog.show();
		result = app.exec();
	}
	g_theApp.ExitInstance();
	return result;
}
