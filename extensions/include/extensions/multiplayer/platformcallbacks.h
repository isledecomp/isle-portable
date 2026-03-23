#pragma once

#include <cstdint>

namespace Multiplayer
{

static const int CONNECTION_STATUS_CONNECTED = 0;
static const int CONNECTION_STATUS_RECONNECTING = 1;
static const int CONNECTION_STATUS_FAILED = 2;
static const int CONNECTION_STATUS_REJECTED = 3;

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

	// Called when the connection status changes (connected, reconnecting, failed).
	virtual void OnConnectionStatusChanged(int p_status) = 0;

	// Called when animation eligibility state changes (location change, player join/leave, etc.).
	// p_json = JSON payload with location, coordinator state, and per-animation slot fill status.
	virtual void OnAnimationsAvailable(const char* p_json) = 0;

	// Called when an animation completes successfully (natural completion, not cancellation).
	// Only fired for actual participants, not observers.
	// p_json = JSON with eventId, animIndex, and participant details (charIndex, displayName).
	virtual void OnAnimationCompleted(const char* p_json) = 0;
};

} // namespace Multiplayer
