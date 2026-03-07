#ifdef __EMSCRIPTEN__

#include "extensions/multiplayer.h"
#include "extensions/multiplayer/networkmanager.h"

#include <emscripten.h>

using namespace Extensions;

extern "C"
{

	EMSCRIPTEN_KEEPALIVE void mp_set_walk_animation(int index)
	{
		Multiplayer::NetworkManager* mgr = MultiplayerExt::GetNetworkManager();
		if (mgr) {
			mgr->SetWalkAnimation(static_cast<uint8_t>(index));
		}
	}

	EMSCRIPTEN_KEEPALIVE void mp_set_idle_animation(int index)
	{
		Multiplayer::NetworkManager* mgr = MultiplayerExt::GetNetworkManager();
		if (mgr) {
			mgr->SetIdleAnimation(static_cast<uint8_t>(index));
		}
	}

	EMSCRIPTEN_KEEPALIVE void mp_trigger_emote(int index)
	{
		Multiplayer::NetworkManager* mgr = MultiplayerExt::GetNetworkManager();
		if (mgr) {
			mgr->SendEmote(static_cast<uint8_t>(index));
		}
	}

	EMSCRIPTEN_KEEPALIVE void mp_toggle_third_person()
	{
		Multiplayer::NetworkManager* mgr = MultiplayerExt::GetNetworkManager();
		if (mgr) {
			Multiplayer::ThirdPersonCamera& cam = mgr->GetThirdPersonCamera();
			if (cam.IsEnabled()) {
				cam.Disable();
			}
			else {
				cam.Enable();
			}
		}
	}

} // extern "C"

#endif
