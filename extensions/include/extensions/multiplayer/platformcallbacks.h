#pragma once

namespace Multiplayer
{

class PlatformCallbacks {
public:
	virtual ~PlatformCallbacks() = default;

	// Called when the visible player count changes (joins, leaves, world transitions).
	// p_count = players visible in current world, or -1 if not in a multiplayer world.
	virtual void OnPlayerCountChanged(int p_count) = 0;
};

} // namespace Multiplayer
