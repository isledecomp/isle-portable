#include "haptic.h"

#include "compat.h"
#include "lego/sources/misc/legoutil.h"
#include "legoinputmanager.h"
#include "misc.h"

#include <emscripten/html5.h>

void Emscripten_HandleRumbleEvent(float p_lowFrequencyRumble, float p_highFrequencyRumble, MxU32 p_milliseconds)
{
	std::visit(
		overloaded{
			[](LegoInputManager::MORTAR_KeyboardID_v p_id) {},
			[](LegoInputManager::MORTAR_MouseID_v p_id) {},
			[p_lowFrequencyRumble, p_highFrequencyRumble, p_milliseconds](LegoInputManager::MORTAR_JoystickID_v p_id) {
				const char* name = MORTAR_GetJoystickNameForID((MORTAR_JoystickID) p_id);
				if (name) {
					MAIN_THREAD_EM_ASM(
						{
							const name = UTF8ToString($0);
							const gamepads = navigator.getGamepads();
							for (const gamepad of gamepads) {
								if (gamepad && gamepad.connected && gamepad.id == name && gamepad.vibrationActuator) {
									gamepad.vibrationActuator.playEffect("dual-rumble", {
										startDelay : 0,
										weakMagnitude : $1,
										strongMagnitude : $2,
										duration : $3,
									});
									break;
								}
							}
						},
						name,
						MORTAR_clamp(p_lowFrequencyRumble, 0, 1),
						MORTAR_clamp(p_highFrequencyRumble, 0, 1),
						p_milliseconds
					);
				}
			},
			[p_milliseconds](LegoInputManager::MORTAR_TouchID_v p_id) {
				MAIN_THREAD_EM_ASM(
					{
						if (navigator.vibrate) {
							navigator.vibrate($0);
						}
					},
					p_milliseconds
				);
			}
		},
		InputManager()->GetLastInputMethod()
	);
}
