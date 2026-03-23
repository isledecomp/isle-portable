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

// clang-format off
static void DispatchBoolEvent(const char* p_name, bool p_value)
{
	MAIN_THREAD_EM_ASM({
		var canvas = Module.canvas;
		if (canvas) {
			canvas.dispatchEvent(new CustomEvent(UTF8ToString($0), {
				detail: { enabled: !!$1 }
			}));
		}
	}, p_name, p_value ? 1 : 0);
}
// clang-format on

void EmscriptenCallbacks::OnThirdPersonChanged(bool p_enabled)
{
	DispatchBoolEvent("thirdPersonChanged", p_enabled);
}

void EmscriptenCallbacks::OnNameBubblesChanged(bool p_enabled)
{
	DispatchBoolEvent("nameBubblesChanged", p_enabled);
}

void EmscriptenCallbacks::OnAllowCustomizeChanged(bool p_enabled)
{
	DispatchBoolEvent("allowCustomizeChanged", p_enabled);
}

void EmscriptenCallbacks::OnConnectionStatusChanged(int p_status)
{
	// clang-format off
	MAIN_THREAD_EM_ASM({
		var canvas = Module.canvas;
		if (canvas) {
			canvas.dispatchEvent(new CustomEvent('connectionStatusChanged', {
				detail: { status: $0 }
			}));
		}
	}, p_status);
	// clang-format on
}

void EmscriptenCallbacks::OnAnimationsAvailable(const char* p_json)
{
	// clang-format off
	MAIN_THREAD_EM_ASM({
		var canvas = Module.canvas;
		if (canvas) {
			canvas.dispatchEvent(new CustomEvent('animationsAvailable', {
				detail: { json: UTF8ToString($0) }
			}));
		}
	}, p_json);
	// clang-format on
}

void EmscriptenCallbacks::OnAnimationCompleted(const char* p_json)
{
	// clang-format off
	MAIN_THREAD_EM_ASM({
		var canvas = Module.canvas;
		if (canvas) {
			canvas.dispatchEvent(new CustomEvent('animationCompleted', {
				detail: { json: UTF8ToString($0) }
			}));
		}
	}, p_json);
	// clang-format on
}

} // namespace Multiplayer

#endif // __EMSCRIPTEN__
