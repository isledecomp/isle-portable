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
			mgr->RequestSetWalkAnimation(static_cast<uint8_t>(index));
		}
	}

	EMSCRIPTEN_KEEPALIVE void mp_set_idle_animation(int index)
	{
		Multiplayer::NetworkManager* mgr = MultiplayerExt::GetNetworkManager();
		if (mgr) {
			mgr->RequestSetIdleAnimation(static_cast<uint8_t>(index));
		}
	}

	EMSCRIPTEN_KEEPALIVE void mp_trigger_emote(int index)
	{
		Multiplayer::NetworkManager* mgr = MultiplayerExt::GetNetworkManager();
		if (mgr) {
			mgr->RequestSendEmote(static_cast<uint8_t>(index));
		}
	}

	EMSCRIPTEN_KEEPALIVE void mp_toggle_third_person()
	{
		Multiplayer::NetworkManager* mgr = MultiplayerExt::GetNetworkManager();
		if (mgr) {
			mgr->RequestToggleThirdPerson();
		}
	}

	EMSCRIPTEN_KEEPALIVE void mp_toggle_name_bubbles()
	{
		Multiplayer::NetworkManager* mgr = MultiplayerExt::GetNetworkManager();
		if (mgr) {
			mgr->RequestToggleNameBubbles();
		}
	}

	EMSCRIPTEN_KEEPALIVE void mp_toggle_allow_customize()
	{
		Multiplayer::NetworkManager* mgr = MultiplayerExt::GetNetworkManager();
		if (mgr) {
			mgr->RequestToggleAllowCustomize();
		}
	}

} // extern "C"

#endif
