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

void merge_dicts(dictionary* dst, dictionary* src)
{
	for (int i = 0; i < src->n; i++) {
		dictionary_set(dst, src->key[i], src->val[i]);
	}
}

struct setting_map {
	const char* key_ini;
	const char* key_app;
	const char type;
};

// mapping from ini key to settings.xml key
const setting_map key_map[] = {
	// Game
	{"isle:diskpath", "disk_path", 's'},
	{"isle:cdpath", "cd_path", 's'},
	{"isle:savepath", "save_path", 's'},
	{"isle:Transition Type", "transition_type", 'i'},
	{"isle:Music", "music", 'b'},
	{"isle:3DSound", "3d_sound", 'b'},

	// Graphics
	{"isle:Island Texture", "island_texture_quality", 'i'},
	{"isle:Island Quality", "island_model_quality", 'i'},
	//{"isle:Max LOD", "max_lod", 'f'},
	//{"isle:Max Allowed Extras", "max_extras", 'i' },
	{"isle:MSAA", "msaa", 'i'},

	// Controls
	{"isle:Touch Scheme", "touch_control_scheme", 'i'},
	{"isle:Haptic", "rumble", 'b'},

	// Extensions
	{"extensions:texture loader", "texture_loader_extension", 'b'},
	{"texture loader:texture path", "texture_loader_path", 's'}
};

struct Config {
	sce::AppSettings* settings;
	dictionary* dict;
	char buffer[128];

#define GetDictInt(x, name) x = iniparser_getint(this->dict, name, x)
#define GetDictFloat(x, name) x = iniparser_getdouble(this->dict, name, x)
#define GetDictString(x, name)                                                                                         \
	{                                                                                                                  \
		const char* val = iniparser_getstring(this->dict, name, nullptr);                                              \
		if (val != nullptr) {                                                                                          \
			x = val;                                                                                                   \
		}                                                                                                              \
	}
#define GetDictBool(x, name) x = iniparser_getboolean(this->dict, name, x)

#define SetDictBool(NAME, VALUE)                                                                                       \
	{                                                                                                                  \
		const char* v = VALUE ? "true" : "false";                                                                      \
		sceClibPrintf("SetIniBool(%s, %s)\n", NAME, v);                                                                \
		iniparser_set(this->dict, NAME, v);                                                                            \
	}
#define SetDictInt(NAME, VALUE)                                                                                        \
	{                                                                                                                  \
		sceClibSnprintf(buffer, sizeof(buffer), "%d", VALUE);                                                          \
		sceClibPrintf("SetIniInt(%s, %d)\n", NAME, VALUE);                                                             \
		iniparser_set(this->dict, NAME, buffer);                                                                       \
	}
#define SetDictFloat(NAME, VALUE)                                                                                      \
	{                                                                                                                  \
		sceClibSnprintf(buffer, sizeof(buffer), "%f", VALUE);                                                          \
		sceClibPrintf("SetIniFloat(%s, %f)\n", NAME, VALUE);                                                           \
		iniparser_set(this->dict, NAME, buffer);                                                                       \
	}
#define SetDictString(NAME, VALUE)                                                                                     \
	{                                                                                                                  \
		sceClibPrintf("SetString(%s, %s)\n", NAME, VALUE);                                                             \
		iniparser_set(this->dict, NAME, VALUE);                                                                        \
	}

	void Init(sce::AppSettings* settings)
	{
		this->settings = settings;
		dict = dictionary_new(0);

		// set defaults
		iniparser_set(this->dict, "isle", nullptr);
		iniparser_set(dict, "extensions", NULL);
		iniparser_set(this->dict, "texture loader", nullptr);

		SetDictString("isle:diskpath", "ux0:data/isledecomp/isle/DATA/disk");
		SetDictString("isle:cdpath", "ux0:data/isledecomp/isle");
		SetDictString("isle:savepath", "ux0:data/isledecomp/isle");
		SetDictInt("isle:MSAA", 4);

		SetDictInt("isle:Display Bit Depth", 32);
		SetDictBool("isle:Flip Surfaces", false);
		SetDictBool("isle:Full Screen", true);
		SetDictBool("isle:Exclusive Full Screen", true);
		SetDictBool("isle:Wide View Angle", true);

		SetDictInt("isle:Transition Type", 3); // 3: Mosaic
		SetDictInt("isle:Touch Scheme", 2);

		SetDictBool("isle:3DSound", true);
		SetDictBool("isle:Music", true);
		SetDictBool("isle:Haptic", true);

		SetDictBool("isle:UseJoystick", true);
		SetDictInt("isle:JoystickIndex", 0);
		SetDictBool("isle:Draw Cursor", true);

		SetDictBool("extensions:texture loader", false);
		SetDictString("texture loader:texture path", "textures/");

		SetDictBool("isle:Back Buffers in Video RAM", true);

		SetDictInt("isle:Island Quality", 2);
		SetDictInt("isle:Island Texture", 1);
		SetDictInt("isle:MSAA", 4);

		SetDictFloat("isle:Max LOD", 3.5);
		SetDictInt("isle:Max Allowed Extras", 20);

		SetDictInt("isle:Aspect Ratio", 0);
		SetDictInt("isle:Horizontal Resolution", 640);
		SetDictInt("isle:Vertical Resolution", 480);
		SetDictFloat("isle:Frame Delta", 10.0f);
	}

	void LoadIni()
	{
		dictionary* ini = iniparser_load(g_iniPath);
		if (ini) {
			merge_dicts(this->dict, ini);
			iniparser_freedict(ini);
		}
	}

	bool SaveIni()
	{
		FILE* fd = fopen(g_iniPath, "w");
		if (fd) {
			iniparser_dump_ini(this->dict, fd);
		}
		else {
			sceClibPrintf("failed to write isle.ini\n");
		}
		return true;
	}

	void ToSettings()
	{
		const int len = sizeof(key_map) / sizeof(key_map[0]);
		for (int i = 0; i < len; i++) {
			const setting_map m = key_map[i];
			switch (m.type) {
			case 'f': // float, AppSettings doesnt have float so just use string
			case 's': {
				const char* value = iniparser_getstring(this->dict, m.key_ini, "");
				this->settings->SetString(m.key_app, value);
				break;
			}
			case 'i': {
				int32_t value = iniparser_getint(this->dict, m.key_ini, 0);
				this->settings->SetInt(m.key_app, value);
				break;
			}
			case 'b': {
				bool value = iniparser_getboolean(this->dict, m.key_ini, 0) == 1;
				this->settings->SetBool(m.key_app, value);
				break;
			}
			default: {
				sceClibPrintf("invalid setting map entry %s %s %c\n", m.key_app, m.key_ini, m.type);
			}
			}
		}
	}

	void FromSettings()
	{
		const int len = sizeof(key_map) / sizeof(key_map[0]);
		for (int i = 0; i < len; i++) {
			const setting_map m = key_map[i];
			switch (m.type) {
			case 'f': // float, AppSettings doesnt have float so just use string
			case 's': {
				const char* def = iniparser_getstring(this->dict, m.key_ini, "");
				this->settings->GetString(m.key_app, this->buffer, sizeof(this->buffer), def);
				iniparser_set(this->dict, m.key_ini, buffer);
				break;
			}
			case 'i': {
				int32_t value = iniparser_getint(this->dict, m.key_ini, 0);
				this->settings->GetInt(m.key_app, &value, value);
				SetDictInt(m.key_ini, value);
				break;
			}
			case 'b': {
				bool value = iniparser_getboolean(this->dict, m.key_ini, 0) == 1;
				this->settings->GetBool(m.key_app, &value, value);
				SetDictBool(m.key_ini, value);
				break;
			}
			default: {
				sceClibPrintf("invalid setting map entry %s %s %c\n", m.key_app, m.key_ini, m.type);
			}
			}
		}
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
	g_config.FromSettings();
	g_config.SaveIni();
	g_fw->RequestShutdown();
	exit_type = 1;
}

void save_and_launch()
{
	g_config.FromSettings();
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
	if (exit_type == 0) {
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
	g_config.Init(g_appSettings);
	g_config.LoadIni();
	g_config.ToSettings();

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
