#pragma once

namespace Multiplayer
{

class PlatformCallbacks {
public:
	virtual ~PlatformCallbacks() = default;

	// Called when the visible player count changes (joins, leaves, world transitions).
	// p_count = players visible in current world, or -1 if not in a multiplayer world.
	virtual void OnPlayerCountChanged(int p_count) = 0;

	// Called when the third-person camera mode changes (toggle or auto-switch).
	virtual void OnThirdPersonChanged(bool p_enabled) = 0;

	// Called when name bubbles visibility changes.
	virtual void OnNameBubblesChanged(bool p_enabled) = 0;

	// Called when the allow-customization setting changes.
	virtual void OnAllowCustomizeChanged(bool p_enabled) = 0;
};

} // namespace Multiplayer
