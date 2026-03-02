#pragma once

#ifdef __EMSCRIPTEN__

#include "extensions/multiplayer/platformcallbacks.h"

namespace Multiplayer
{

class EmscriptenCallbacks : public PlatformCallbacks {
public:
	void OnPlayerCountChanged(int p_count) override;
};

} // namespace Multiplayer

#endif // __EMSCRIPTEN__
