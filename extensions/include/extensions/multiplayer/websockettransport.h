#pragma once

#ifdef __EMSCRIPTEN__

#include "extensions/multiplayer/networktransport.h"

#include <string>

namespace Multiplayer
{

class WebSocketTransport : public NetworkTransport {
public:
	WebSocketTransport(const std::string& p_relayBaseUrl);
	~WebSocketTransport() override;

	void Connect(const char* p_roomId) override;
	void Disconnect() override;
	bool IsConnected() const override;
	void Send(const uint8_t* p_data, size_t p_length) override;
	size_t Receive(std::function<void(const uint8_t*, size_t)> p_callback) override;

private:
	std::string m_relayBaseUrl;
	int m_socketId;
	volatile int32_t m_connectedFlag; // Shared with JS main thread via Atomics
	uint8_t m_recvBuf[8192];
};

} // namespace Multiplayer

#endif // __EMSCRIPTEN__
