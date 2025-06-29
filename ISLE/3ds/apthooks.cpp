#include "apthooks.h"

#include "legomain.h"
#include "misc.h"

aptHookCookie g_aptCookie;

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

void N3DS_SetupAptHooks()
{
	aptHook(&g_aptCookie, N3DS_AptHookCallback, NULL);
}
