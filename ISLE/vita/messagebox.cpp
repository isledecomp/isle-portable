#include "messagebox.h"

#include "../../miniwin/src/d3drm/backends/gxm/gxm_context.h"

#include <psp2/common_dialog.h>
#include <psp2/message_dialog.h>

bool Vita_ShowSimpleMessageBox(
	MORTAR_MessageBoxFlags flags,
	const char* title,
	const char* message,
	MORTAR_Window* window
)
{
	int ret;
	SceMsgDialogParam param;
	SceMsgDialogUserMessageParam msgParam;
	SceMsgDialogButtonsParam buttonParam;
	SceMsgDialogResult dialog_result;
	SceCommonDialogErrorCode init_result;
	bool setup_minimal_gxm = false;

	MORTAR_zero(param);
	sceMsgDialogParamInit(&param);
	param.mode = SCE_MSG_DIALOG_MODE_USER_MSG;

	MORTAR_zero(msgParam);
	char message_data[0x1000];
	MORTAR_snprintf(message_data, sizeof(message_data), "%s\r\n\r\n%s", title, message);

	msgParam.msg = (const SceChar8*) message_data;
	msgParam.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK;
	param.userMsgParam = &msgParam;

	if (!gxm) {
		gxm = (GXMContext*) MORTAR_malloc(sizeof(GXMContext));
	}
	if (ret = gxm->init(SCE_GXM_MULTISAMPLE_NONE); ret < 0) {
		return false;
	}

	init_result = (SceCommonDialogErrorCode) sceMsgDialogInit(&param);
	if (init_result >= 0) {
		while (sceMsgDialogGetStatus() == SCE_COMMON_DIALOG_STATUS_RUNNING) {
			gxm->clear(0, 0, 0, true);
			gxm->swap_display();
		}
		MORTAR_zero(dialog_result);
		sceMsgDialogGetResult(&dialog_result);
		sceMsgDialogTerm();
		return dialog_result.buttonId == SCE_MSG_DIALOG_BUTTON_ID_OK;
	}
	else {
		return false;
	}
	return true;
}
