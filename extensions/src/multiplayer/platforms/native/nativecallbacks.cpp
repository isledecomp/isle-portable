#ifndef __EMSCRIPTEN__

#include "extensions/multiplayer/platforms/native/nativecallbacks.h"

#include <SDL3/SDL_log.h>

namespace Multiplayer
{

void NativeCallbacks::OnPlayerCountChanged(int p_count)
{
	if (p_count < 0) {
		SDL_Log("[Multiplayer] Left multiplayer world");
	}
	else {
		SDL_Log("[Multiplayer] Player count changed: %d", p_count);
	}
}

void NativeCallbacks::OnThirdPersonChanged(bool p_enabled)
{
	SDL_Log("[Multiplayer] Third person camera: %s", p_enabled ? "enabled" : "disabled");
}

void NativeCallbacks::OnNameBubblesChanged(bool p_enabled)
{
	SDL_Log("[Multiplayer] Name bubbles: %s", p_enabled ? "enabled" : "disabled");
}

void NativeCallbacks::OnAllowCustomizeChanged(bool p_enabled)
{
	SDL_Log("[Multiplayer] Allow customization: %s", p_enabled ? "enabled" : "disabled");
}

void NativeCallbacks::OnConnectionStatusChanged(int p_status)
{
	const char* statusStr = "unknown";
	switch (p_status) {
	case CONNECTION_STATUS_CONNECTED:
		statusStr = "connected";
		break;
	case CONNECTION_STATUS_RECONNECTING:
		statusStr = "reconnecting";
		break;
	case CONNECTION_STATUS_FAILED:
		statusStr = "failed";
		break;
	case CONNECTION_STATUS_REJECTED:
		statusStr = "rejected (room full)";
		break;
	}
	SDL_Log("[Multiplayer] Connection status: %s", statusStr);
}

void NativeCallbacks::OnAnimationsAvailable(const char* p_json)
{
	(void) p_json;
}

void NativeCallbacks::OnAnimationCompleted(const char* p_json)
{
	SDL_Log("[Multiplayer] Animation completed: %s", p_json);
}

} // namespace Multiplayer

#endif // !__EMSCRIPTEN__
