#include "apthooks.h"

#include "../isleapp.h"
#include "legomain.h"
#include "misc.h"

void N3DS_AptHookCallback(APT_HookType hookType, void* param)
{
	switch (hookType) {
	case APTHOOK_ONSLEEP:
	case APTHOOK_ONSUSPEND:
		Lego()->Pause();
		break;
	case APTHOOK_ONWAKEUP:
	case APTHOOK_ONRESTORE:
		Lego()->Resume();
		break;
	case APTHOOK_ONEXIT:
		Lego()->CloseMainWindow();
		break;
	default:
		break;
	}
}
