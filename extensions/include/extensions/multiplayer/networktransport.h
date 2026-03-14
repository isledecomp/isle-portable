#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace Multiplayer
{

class NetworkTransport {
public:
	virtual ~NetworkTransport() = default;

	virtual void Connect(const char* p_roomId) = 0;
	virtual void Disconnect() = 0;
	virtual bool IsConnected() const = 0;
	virtual bool WasDisconnected() const = 0;

	// Send binary data to all peers via relay
	virtual void Send(const uint8_t* p_data, size_t p_length) = 0;

	// Drain received messages. Callback called for each message.
	// Returns number of messages dequeued.
	virtual size_t Receive(std::function<void(const uint8_t*, size_t)> p_callback) = 0;
};

} // namespace Multiplayer
