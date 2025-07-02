#include <psp2/kernel/clib.h>

#include <paf.h>


paf::Framework *g_fw;
void *g_rootPage;


void loadPluginCB(paf::Plugin *plugin){

	{
		paf::Plugin::PageOpenParam pageOpenParam;
		pageOpenParam.option = paf::Plugin::PageOption_None;

		paf::ui::Scene *pScene = plugin->PageOpen("page_main", pageOpenParam);

		g_rootPage = pScene;

		paf::ui::Widget *pPlane = pScene->FindChild("plane_sample_black");
		paf::ui::ProgressBar *pProgressbar = (paf::ui::ProgressBar *)pPlane->FindChild("progressbar");

		pProgressbar->SetMinValue(0);
		pProgressbar->SetMaxValue(100);

		pProgressbar->SetValue(24, true);

		wchar_t *msg_progressbar_text = plugin->GetString("msg_progressbar_text");

		paf::ui::Text *pText = (paf::ui::Text *)pPlane->FindChild("progressbar_text");
		pText->SetString(paf::wstring(msg_progressbar_text));
	}
}

int paf_main(void){

	{
		paf::Framework::InitParam fwParam;
		fwParam.mode = paf::Framework::Mode_Normal;

		paf::Framework *paf_fw = new paf::Framework(fwParam);
		if(paf_fw != NULL){
			g_fw = paf_fw;

			paf_fw->LoadCommonResourceSync();

			paf::Plugin::InitParam pluginParam;

			pluginParam.name          = "sample_plugin";
			pluginParam.caller_name   = "__main__";
			pluginParam.resource_file = "app0:/sample_plugin.rco";
			pluginParam.init_func     = NULL;
			pluginParam.start_func    = loadPluginCB;
			pluginParam.stop_func     = NULL;
			pluginParam.exit_func     = NULL;

			paf::Plugin::LoadSync(pluginParam);
			paf_fw->Run();
		}
	}

	sceClibPrintf("[SAMPLE] Failed to run PAF instance\n");

	exit(0);
	return 0;
}