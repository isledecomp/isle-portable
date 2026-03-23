#pragma once

#ifndef __EMSCRIPTEN__

#include "extensions/multiplayer/platformcallbacks.h"

namespace Multiplayer
{

class NativeCallbacks : public PlatformCallbacks {
public:
	void OnPlayerCountChanged(int p_count) override;
	void OnThirdPersonChanged(bool p_enabled) override;
	void OnNameBubblesChanged(bool p_enabled) override;
	void OnAllowCustomizeChanged(bool p_enabled) override;
	void OnConnectionStatusChanged(int p_status) override;
	void OnAnimationsAvailable(const char* p_json) override;
	void OnAnimationCompleted(const char* p_json) override;
};

} // namespace Multiplayer

#endif // !__EMSCRIPTEN__
