// clang-format off
#include <stdarg.h>
#include <paf.h>
// clang-format on
#include <psp2/kernel/clib.h>

paf::Framework* g_fw;
paf::ui::Scene* g_rootPage;
paf::Plugin* g_plugin;

void loadPluginCB(paf::Plugin* plugin)
{
	g_plugin = plugin;
	plugin->SetLocale(Locale_EN);

	paf::Plugin::PageOpenParam pageOpenParam;
	pageOpenParam.option = paf::Plugin::PageOption_None;
	paf::ui::Scene* pScene = plugin->PageOpen("page_main", pageOpenParam);
	g_rootPage = pScene;

	const auto saveAndExitButton = static_cast<paf::ui::Button*>(pScene->FindChild("save_exit_button"));
	const auto startAndExitButton = static_cast<paf::ui::Button*>(pScene->FindChild("start_game_button"));
	const auto exitButton = static_cast<paf::ui::Button*>(pScene->FindChild("exit_button"));
}

int paf_main(void)
{
	paf::Framework::InitParam fwParam;
	fwParam.mode = paf::Framework::Mode_Normal;

	paf::Framework* paf_fw = new paf::Framework(fwParam);
	if (paf_fw != NULL) {
		g_fw = paf_fw;

		paf_fw->LoadCommonResourceSync();

		paf::Plugin::InitParam pluginParam;

		pluginParam.name = "config_plugin";
		pluginParam.caller_name = "__main__";
		pluginParam.resource_file = "app0:/config_plugin.rco";
		pluginParam.init_func = NULL;
		pluginParam.start_func = loadPluginCB;
		pluginParam.stop_func = NULL;
		pluginParam.exit_func = NULL;

		paf::Plugin::LoadSync(pluginParam);
		paf_fw->Run();
	}

	sceClibPrintf("[SAMPLE] Failed to run PAF instance\n");

	exit(0);
	return 0;
}
