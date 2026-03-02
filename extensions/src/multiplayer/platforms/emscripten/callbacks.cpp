#ifdef __EMSCRIPTEN__

#include "extensions/multiplayer/platforms/emscripten/callbacks.h"

#include <emscripten.h>

namespace Multiplayer
{

void EmscriptenCallbacks::OnPlayerCountChanged(int p_count)
{
	// clang-format off
	MAIN_THREAD_EM_ASM({
		var canvas = Module.canvas;
		if (canvas) {
			canvas.dispatchEvent(new CustomEvent('playerCountChanged', {
				detail: { count: $0 < 0 ? null : $0 }
			}));
		}
	}, p_count);
	// clang-format on
}

} // namespace Multiplayer

#endif // __EMSCRIPTEN__
