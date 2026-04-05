#pragma once

#ifndef __EMSCRIPTEN__

#include "extensions/multiplayer/networktransport.h"
#include "mxcriticalsection.h"
#include "mxthread.h"

#include <atomic>
#include <deque>
#include <string>
#include <vector>

struct lws_context;
struct lws;

namespace Multiplayer
{

class LwsTransport;

class LwsServiceThread : public MxThread {
public:
	LwsServiceThread() : m_transport(nullptr) {}
	MxResult Run() override;
	void SetTransport(LwsTransport* p_transport) { m_transport = p_transport; }

private:
	LwsTransport* m_transport;
};

class LwsTransport : public NetworkTransport {
	friend class LwsServiceThread;

public:
	LwsTransport(const std::string& p_relayBaseUrl);
	~LwsTransport() override;

	void Connect(const char* p_roomId) override;
	void Disconnect() override;
	bool IsConnected() const override;
	bool WasDisconnected() const override;
	bool WasRejected() const override;
	void Send(const uint8_t* p_data, size_t p_length) override;
	size_t Receive(std::function<void(const uint8_t*, size_t)> p_callback) override;

	// Called from static lws callback trampoline
	int HandleLwsEvent(struct lws* p_wsi, int p_reason, void* p_in, size_t p_len);

private:
	void ServiceLoop();

	std::string m_relayBaseUrl;
	struct lws_context* m_context;
	std::atomic<struct lws*> m_wsi;
	std::atomic<bool> m_connected;
	std::atomic<bool> m_disconnected;
	std::atomic<bool> m_wasEverConnected;

	MxCriticalSection m_sendCS;
	MxCriticalSection m_recvCS;
	std::deque<std::vector<uint8_t>> m_sendQueue;
	std::deque<std::vector<uint8_t>> m_recvQueue;
	std::vector<uint8_t> m_fragment;

	LwsServiceThread* m_serviceThread;
	std::atomic<bool> m_wantWritable;
};

} // namespace Multiplayer

#endif // !__EMSCRIPTEN__
