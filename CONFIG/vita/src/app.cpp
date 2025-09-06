#include "pafinc.h"

#include <app_settings.h>
#include <iniparser.h>
#include <psp2/appmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/sysmodule.h>

int sceLibcHeapSize = 10 * 1024 * 1024;

const char* g_iniPath = "ux0:data/isledecomp/isle/isle.ini";

paf::Framework* g_fw;
paf::Plugin* g_configPlugin;
sce::AppSettings* g_appSettings;
sce::AppSettings::Interface* g_appSetIf;

struct Config {
	paf::string m_base_path;
	paf::string m_cd_path;
	paf::string m_save_path;
	int m_transition_type;
	int m_texture_quality;
	int m_model_quality;
	int m_msaa;
	int m_touch_scheme;
	bool m_wide_view_angle;
	bool m_music;
	bool m_3d_sound;
	bool m_haptic;
	bool m_draw_cursor;
	bool m_texture_load;
	paf::string m_texture_path;
	float m_max_lod;
	int m_max_actors;
	float m_frame_delta;

	void Init()
	{
		m_frame_delta = 10.0f;
		m_transition_type = 3; // 3: Mosaic
		m_wide_view_angle = true;
		m_music = true;
		m_3d_sound = true;
		m_haptic = true;
		m_touch_scheme = 2;
		m_texture_load = true;
		m_texture_path = "/textures/";
		m_model_quality = 2;
		m_texture_quality = 1;
		m_msaa = 4;
		m_max_lod = 3.5f;
		m_max_actors = 20;
	}

	void LoadIni()
	{
		dictionary* dict = iniparser_load(g_iniPath);
		if (!dict) {
			dict = dictionary_new(0);
		}

#define GET_INT(x, name) x = iniparser_getint(dict, name, x)
#define GET_FLOAT(x, name) x = iniparser_getdouble(dict, name, x)
#define GET_STRING(x, name) do { \
	const char* val = iniparser_getstring(dict, name, nullptr); \
	if(val != nullptr) x = val; \
} while(0)
#define GET_BOOLEAN(x, name) x = iniparser_getboolean(dict, name, x)

		GET_STRING(m_base_path, "isle:diskpath");
		GET_STRING(m_cd_path, "isle:cdpath");
		GET_STRING(m_save_path, "isle:savepath");

		// m_display_bit_depth = iniparser_getint(dict, "isle:Display Bit Depth", -1);
		// GET_BOOLEAN(m_flip_surfaces, "isle:Flip Surfaces");
		// GET_BOOLEAN(m_full_screen, "isle:Full Screen");
		// GET_BOOLEAN(m_exclusive_full_screen, "isle:Exclusive Full Screen");
		GET_INT(m_transition_type, "isle:Transition Type");
		GET_INT(m_touch_scheme, "isle:Touch Scheme");
		// GET_BOOLEAN(m_3d_video_ram, "isle:Back Buffers in Video RAM");
		GET_BOOLEAN(m_wide_view_angle, "isle:Wide View Angle");
		GET_BOOLEAN(m_3d_sound, "isle:3DSound");
		GET_BOOLEAN(m_draw_cursor, "isle:Draw Cursor");
		GET_INT(m_model_quality, "isle:Island Quality");
		GET_INT(m_texture_quality, "isle:Island Texture");
		GET_INT(m_msaa, "isle:MSAA");
		// GET_BOOLEAN(m_use_joystick, "isle:UseJoystick");
		GET_BOOLEAN(m_haptic, "isle:Haptic");
		GET_BOOLEAN(m_music, "isle:Music");
		// GET_INT(m_joystick_index, "isle:JoystickIndex");
		GET_FLOAT(m_max_lod, "isle:Max LOD");
		GET_INT(m_max_actors, "isle:Max Allowed Extras");
		GET_BOOLEAN(m_texture_load, "extensions:texture loader");
		GET_STRING(m_texture_path, "texture loader:texture path");
		// GET_INT(m_aspect_ratio, "isle:Aspect Ratio");
		// GET_INT(m_x_res, "isle:Horizontal Resolution");
		// GET_INT(m_y_res, "isle:Vertical Resolution");
		GET_FLOAT(m_frame_delta, "isle:Frame Delta");
#undef GET_INT
#undef GET_FLOAT
#undef GET_STRING
#undef GET_BOOLEAN
		iniparser_freedict(dict);
	}

	bool SaveIni()
	{
		dictionary* dict = dictionary_new(0);

		char buffer[128];
#define SetIniBool(NAME, VALUE) iniparser_set(dict, NAME, VALUE ? "true" : "false")
#define SetIniInt(NAME, VALUE)                                                                                         \
	{                                                                                                                  \
		sceClibPrintf(buffer, "%d", VALUE);                                                                            \
		iniparser_set(dict, NAME, buffer);                                                                             \
	}
#define SetIniFloat(NAME, VALUE)                                                                                       \
	{                                                                                                                  \
		sceClibPrintf(buffer, "%f", VALUE);                                                                            \
		iniparser_set(dict, NAME, buffer);                                                                             \
	}
#define SetString(NAME, VALUE) iniparser_set(dict, NAME, VALUE)

		SetIniInt("isle:Display Bit Depth", 32);
		SetIniBool("isle:Flip Surfaces", false);
		SetIniBool("isle:Full Screen", true);
		SetIniBool("isle:Exclusive Full Screen", true);
		SetIniBool("isle:Wide View Angle", true); // option?

		SetIniInt("isle:Transition Type", m_transition_type);
		SetIniInt("isle:Touch Scheme", m_touch_scheme);

		SetIniBool("isle:3DSound", m_3d_sound);
		SetIniBool("isle:Music", m_music);
		SetIniBool("isle:Haptic", m_haptic);

		SetIniBool("isle:UseJoystick", true);
		SetIniInt("isle:JoystickIndex", 0);
		SetIniBool("isle:Draw Cursor", m_draw_cursor);

		SetIniBool("extensions:texture loader", m_texture_load);
		SetString("texture loader:texture path", m_texture_path.c_str());

		SetIniBool("isle:Back Buffers in Video RAM", true);

		SetIniInt("isle:Island Quality", m_model_quality);
		SetIniInt("isle:Island Texture", m_texture_quality);
		SetIniInt("isle:MSAA", m_msaa);

		SetIniFloat("isle:Max LOD", m_max_lod);
		SetIniInt("isle:Max Allowed Extras", m_max_actors);

		SetIniInt("isle:Aspect Ratio", 0);
		SetIniInt("isle:Horizontal Resolution", 640);
		SetIniInt("isle:Vertical Resolution", 480);
		SetIniFloat("isle:Frame Delta", 10.0f);

#undef SetIniBool
#undef SetIniInt
#undef SetIniFloat
#undef SetString

		FILE* fd = fopen(g_iniPath, "w");
		if (fd) {
			iniparser_dump_ini(dict, fd);
		} else {
			sceClibPrintf("failed to write isle.ini\n");
		}
		iniparser_freedict(dict);

		return true;
	}

	void ToSettings(sce::AppSettings* appSettings)
	{
		appSettings->SetString("data_path", this->m_base_path.c_str());
		appSettings->SetString("save_path", this->m_save_path.c_str());
		appSettings->SetInt("transition_type", this->m_transition_type);
		appSettings->SetBool("music", this->m_music);
		appSettings->SetBool("3d_sound", this->m_3d_sound);
		appSettings->SetInt("island_texture_quality", this->m_texture_quality);
		appSettings->SetInt("island_model_quality", this->m_model_quality);
		appSettings->SetInt("msaa", this->m_msaa);
		appSettings->SetInt("touch_control_scheme", this->m_touch_scheme);
		appSettings->SetBool("rumble", this->m_haptic);
		appSettings->SetBool("texture_loader_extension", this->m_texture_load);
		appSettings->SetString("texture_loader_path", this->m_texture_path.c_str());
	}

	void FromSettings(sce::AppSettings* appSettings) {
		char text_buf[255];

		#define GET_STRING(x, name) appSettings->GetString(name, text_buf, sizeof(text_buf), x.c_str()); x = text_buf;

		GET_STRING(this->m_base_path, "data_path");
		GET_STRING(this->m_save_path, "save_path");
		appSettings->GetInt("transition_type", &this->m_transition_type, this->m_transition_type);
		printf("this->m_transition_type: %d\n", this->m_transition_type);
		appSettings->GetBool("music", &this->m_music, this->m_music);
		appSettings->GetBool("3d_sound", &this->m_3d_sound, this->m_3d_sound);
		appSettings->GetInt("island_texture_quality", &this->m_texture_quality, this->m_texture_quality);
		appSettings->GetInt("island_model_quality", &this->m_model_quality, this->m_model_quality);
		appSettings->GetInt("msaa", &this->m_msaa, this->m_msaa);
		appSettings->GetInt("touch_control_scheme", &this->m_touch_scheme, this->m_touch_scheme);
		appSettings->GetBool("rumble", &this->m_haptic, this->m_haptic);
		appSettings->GetBool("texture_loader_extension", &this->m_texture_load, this->m_texture_load);
		GET_STRING(this->m_texture_path, "texture_loader_path");

		#undef GET_STRING
	}
};

Config g_config;

paf::Plugin* load_config_plugin(paf::Framework* paf_fw)
{
	paf::Plugin::InitParam pluginParam;
	pluginParam.name = "config_plugin";
	pluginParam.caller_name = "__main__";
	pluginParam.resource_file = "app0:/config_plugin.rco";
	pluginParam.init_func = NULL;
	pluginParam.start_func = NULL;
	pluginParam.stop_func = NULL;
	pluginParam.exit_func = NULL;
	paf::Plugin::LoadSync(pluginParam);
	return paf_fw->FindPlugin("config_plugin");
}

int load_app_settings_plugin()
{
	paf::Plugin::InitParam pluginParam;
	sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_BXCE);
	sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_INI_FILE_PROCESSOR);
	sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_COMMON_GUI_DIALOG);

	pluginParam.name = "app_settings_plugin";
	pluginParam.resource_file = "vs0:vsh/common/app_settings_plugin.rco";
	pluginParam.caller_name = "__main__";
	pluginParam.set_param_func = sce::AppSettings::PluginSetParamCB;
	pluginParam.init_func = sce::AppSettings::PluginInitCB;
	pluginParam.start_func = sce::AppSettings::PluginStartCB;
	pluginParam.stop_func = sce::AppSettings::PluginStopCB;
	pluginParam.exit_func = sce::AppSettings::PluginExitCB;
	pluginParam.module_file = "vs0:vsh/common/app_settings.suprx";
	pluginParam.draw_priority = 0x96;
	paf::Plugin::LoadSync(pluginParam);
	return 0;
}

int exit_type = 0;

void save_and_exit()
{
	g_config.FromSettings(g_appSettings);
	g_config.SaveIni();
	g_fw->RequestShutdown();
	exit_type = 1;
}

void save_and_launch()
{
	g_config.FromSettings(g_appSettings);
	g_config.SaveIni();
	g_fw->RequestShutdown();
	exit_type = 2;
}

void CBOnStartPageTransition(const char* elementId, int32_t type)
{
}

void CBOnPageActivate(const char* elementId, int32_t type)
{
}

void CBOnPageDeactivate(const char* elementId, int32_t type)
{
}

int32_t CBOnCheckVisible(const char* elementId, bool* pIsVisible)
{
	*pIsVisible = true;
	return SCE_OK;
}

int32_t CBOnPreCreate(const char* elementId, sce::AppSettings::Element* element)
{
	return SCE_OK;
}

int32_t CBOnPostCreate(const char* elementId, paf::ui::Widget* widget)
{
	return SCE_OK;
}

int32_t CBOnPress(const char* elementId, const char* newValue)
{
	if (sce_paf_strcmp(elementId, "save_exit_button") == 0) {
		save_and_exit();
		return SCE_OK;
	}

	if (sce_paf_strcmp(elementId, "save_launch_button") == 0) {
		save_and_launch();
		return SCE_OK;
	}

	sceClibPrintf("OnPress %s %s\n", elementId, newValue);
	return SCE_OK;
}

int32_t CBOnPress2(const char* elementId, const char* newValue)
{
	return SCE_OK;
}

void CBOnTerm(int32_t result)
{
	if(exit_type == 0) {
		sceKernelExitProcess(0);
	}
}

const wchar_t* CBOnGetString(const char* elementId)
{
	wchar_t* res = g_configPlugin->GetString(elementId);
	if (res[0] != 0) {
		return res;
	}
	return L"unknown string";
}

int32_t CBOnGetSurface(paf::graph::Surface** surf, const char* elementId)
{
	return SCE_OK;
}

void open_settings()
{
	g_config.Init();
	g_config.LoadIni();
	g_config.ToSettings(g_appSettings);

	sce::AppSettings::InterfaceCallbacks ifCb;
	ifCb.onStartPageTransitionCb = CBOnStartPageTransition;
	ifCb.onPageActivateCb = CBOnPageActivate;
	ifCb.onPageDeactivateCb = CBOnPageDeactivate;
	ifCb.onCheckVisible = CBOnCheckVisible;
	ifCb.onPreCreateCb = CBOnPreCreate;
	ifCb.onPostCreateCb = CBOnPostCreate;
	ifCb.onPressCb = CBOnPress;
	ifCb.onPressCb2 = CBOnPress2;
	ifCb.onTermCb = CBOnTerm;
	ifCb.onGetStringCb = (sce::AppSettings::InterfaceCallbacks::GetStringCallback) CBOnGetString;
	ifCb.onGetSurfaceCb = CBOnGetSurface;

	paf::wstring msg_save_exit(g_configPlugin->GetString("msg_save_exit"));
	paf::wstring msg_save_launch(g_configPlugin->GetString("msg_save_launch"));
	paf::wstring msg_exit(g_configPlugin->GetString("msg_exit"));

	paf::Plugin* appSetPlug = paf::Plugin::Find("app_settings_plugin");
	g_appSetIf = (sce::AppSettings::Interface*) appSetPlug->GetInterface(1);
	g_appSetIf->Show(&ifCb);
	g_appSetIf->AddFooterButton("save_exit_button", &msg_save_exit, 1);
	g_appSetIf->AddFooterButton("save_launch_button", &msg_save_launch, 2);
	g_appSetIf->ShowFooter();
}

int paf_main(void)
{
	paf::Framework::InitParam fwParam;
	fwParam.mode = paf::Framework::Mode_Normal;

	paf::Framework* paf_fw = new paf::Framework(fwParam);
	g_fw = paf_fw;

	paf_fw->LoadCommonResourceSync();
	load_app_settings_plugin();
	paf::Plugin* configPlugin = load_config_plugin(paf_fw);
	g_configPlugin = configPlugin;
	configPlugin->SetLocale(Locale_EN);

	size_t fileSize = 0;
	const char* mimeType = nullptr;
	auto settingsXmlFile = configPlugin->GetResource()->GetFile("settings.xml", &fileSize, &mimeType);

	sce::AppSettings::InitParam settingsParam;
	settingsParam.xml_file = settingsXmlFile;
	settingsParam.alloc_cb = sce_paf_malloc;
	settingsParam.free_cb = sce_paf_free;
	settingsParam.realloc_cb = sce_paf_realloc;
	settingsParam.safemem_offset = 0;
	settingsParam.safemem_size = 0x400;

	sce::AppSettings::GetInstance(settingsParam, &g_appSettings);
	g_appSettings->Initialize();

	open_settings();
	paf_fw->Run();

	if (exit_type == 2) {
		int ret = sceAppMgrLoadExec("app0:/eboot.bin", NULL, NULL);
		printf("sceAppMgrLoadExec: %08x\n", ret);
	}
	return 0;
}
