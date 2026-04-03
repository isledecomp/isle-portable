#ifndef __EMSCRIPTEN__

#include "extensions/multiplayer/platforms/native/lwstransport.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>
#include <libwebsockets.h>

namespace Multiplayer
{

static int LwsCallback(struct lws* p_wsi, enum lws_callback_reasons p_reason, void* p_user, void* p_in, size_t p_len)
{
	LwsTransport* transport = static_cast<LwsTransport*>(lws_get_opaque_user_data(p_wsi));
	if (transport) {
		return transport->HandleLwsEvent(p_wsi, static_cast<int>(p_reason), p_in, p_len);
	}
	return 0;
}

static constexpr size_t LWS_RX_BUFFER_SIZE = 8192;
static constexpr int LWS_SERVICE_TIMEOUT_MS = 50;

// clang-format off
static const struct lws_protocols s_protocols[] = {
	{"lws-multiplayer", LwsCallback, 0, LWS_RX_BUFFER_SIZE},
	LWS_PROTOCOL_LIST_TERM
};
// clang-format on

MxResult LwsServiceThread::Run()
{
	while (IsRunning()) {
		m_transport->ServiceLoop();
	}
	return MxThread::Run();
}

LwsTransport::LwsTransport(const std::string& p_relayBaseUrl)
	: m_relayBaseUrl(p_relayBaseUrl), m_context(nullptr), m_wsi(nullptr), m_connected(false), m_disconnected(false),
	  m_wasEverConnected(false), m_serviceThread(nullptr), m_wantWritable(false)
{
}

LwsTransport::~LwsTransport()
{
	Disconnect();
}

void LwsTransport::Connect(const char* p_roomId)
{
	if (m_context) {
		Disconnect();
	}

	m_disconnected.store(false);
	m_wasEverConnected.store(false);

	// lws_parse_uri modifies the string in place, so we need a mutable copy
	std::string fullUrl = m_relayBaseUrl + "/room/" + p_roomId;
	std::vector<char> urlBuf(fullUrl.begin(), fullUrl.end());
	urlBuf.push_back('\0');

	const char* protocol = nullptr;
	const char* address = nullptr;
	const char* path = nullptr;
	int port = 0;

	if (lws_parse_uri(&urlBuf[0], &protocol, &address, &port, &path)) {
		SDL_Log("[Multiplayer] Failed to parse relay URL: %s", fullUrl.c_str());
		m_disconnected.store(true);
		return;
	}

	bool useSSL = (SDL_strcmp(protocol, "wss") == 0 || SDL_strcmp(protocol, "https") == 0);
	SDL_Log("[Multiplayer] Connecting to %s://%s:%d/%s (SSL=%d)", protocol, address, port, path, useSSL);

	lws_set_log_level(LLL_ERR | LLL_WARN, nullptr);

	struct lws_context_creation_info ctxInfo;
	SDL_memset(&ctxInfo, 0, sizeof(ctxInfo));
	ctxInfo.port = CONTEXT_PORT_NO_LISTEN;
	ctxInfo.protocols = s_protocols;
	if (useSSL) {
		ctxInfo.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	}

	m_context = lws_create_context(&ctxInfo);
	if (!m_context) {
		SDL_Log("[Multiplayer] Failed to create lws context");
		m_disconnected.store(true);
		return;
	}

	// path from lws_parse_uri does not include the leading '/', so prepend it
	std::string fullPath = std::string("/") + path;

	struct lws_client_connect_info connInfo;
	SDL_memset(&connInfo, 0, sizeof(connInfo));
	connInfo.context = m_context;
	connInfo.address = address;
	connInfo.port = port;
	connInfo.path = fullPath.c_str();
	connInfo.host = address;
	connInfo.origin = address;
	connInfo.ssl_connection = useSSL ? (LCCSCF_USE_SSL | LCCSCF_ALLOW_INSECURE) : 0;
	connInfo.local_protocol_name = s_protocols[0].name;
	connInfo.opaque_user_data = this;

	struct lws* wsi = lws_client_connect_via_info(&connInfo);
	if (!wsi) {
		SDL_Log("[Multiplayer] Failed to initiate WebSocket connection to %s:%d%s", address, port, fullPath.c_str());
		lws_context_destroy(m_context);
		m_context = nullptr;
		m_disconnected.store(true);
		return;
	}

	m_wsi.store(wsi);
	m_serviceThread = new LwsServiceThread();
	m_serviceThread->SetTransport(this);
	if (m_serviceThread->Start(0, 0) != SUCCESS) {
		SDL_Log("[Multiplayer] Failed to start WebSocket service thread");
		delete m_serviceThread;
		m_serviceThread = nullptr;
		m_wsi.store(nullptr);
		lws_context_destroy(m_context);
		m_context = nullptr;
		m_disconnected.store(true);
		return;
	}
}

void LwsTransport::Disconnect()
{
	if (m_context) {
		lws_cancel_service(m_context);
		m_serviceThread->Terminate();
		delete m_serviceThread;
		m_serviceThread = nullptr;

		lws_context_destroy(m_context);
		m_context = nullptr;
	}

	m_wsi.store(nullptr);
	m_connected.store(false);
	m_wantWritable.store(false);

	m_sendCS.Enter();
	m_sendQueue.clear();
	m_sendCS.Leave();

	m_recvCS.Enter();
	m_recvQueue.clear();
	m_recvCS.Leave();

	m_fragment.clear();
}

bool LwsTransport::IsConnected() const
{
	return m_connected.load();
}

bool LwsTransport::WasDisconnected() const
{
	return m_disconnected.load();
}

bool LwsTransport::WasRejected() const
{
	return m_disconnected.load() && !m_wasEverConnected.load();
}

void LwsTransport::Send(const uint8_t* p_data, size_t p_length)
{
	if (!m_connected.load() || !m_wsi.load()) {
		return;
	}

	std::vector<uint8_t> buf(LWS_PRE + p_length);
	SDL_memcpy(&buf[LWS_PRE], p_data, p_length);

	m_sendCS.Enter();
	m_sendQueue.push_back(std::move(buf));
	m_sendCS.Leave();

	m_wantWritable.store(true);
	if (m_context) {
		lws_cancel_service(m_context);
	}
}

size_t LwsTransport::Receive(std::function<void(const uint8_t*, size_t)> p_callback)
{
	if (!m_context) {
		return 0;
	}

	std::deque<std::vector<uint8_t>> local;

	m_recvCS.Enter();
	local.swap(m_recvQueue);
	m_recvCS.Leave();

	for (const auto& msg : local) {
		p_callback(msg.data(), msg.size());
	}

	return local.size();
}

void LwsTransport::ServiceLoop()
{
	if (m_wantWritable.exchange(false)) {
		struct lws* wsi = m_wsi.load();
		if (wsi) {
			lws_callback_on_writable(wsi);
		}
	}

	lws_service(m_context, LWS_SERVICE_TIMEOUT_MS);
}

int LwsTransport::HandleLwsEvent(struct lws* p_wsi, int p_reason, void* p_in, size_t p_len)
{
	switch (p_reason) {
	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		SDL_Log("[Multiplayer] WebSocket connection established");
		m_connected.store(true);
		m_wasEverConnected.store(true);
		break;

	case LWS_CALLBACK_CLIENT_RECEIVE:
		m_fragment.insert(m_fragment.end(), static_cast<uint8_t*>(p_in), static_cast<uint8_t*>(p_in) + p_len);
		if (lws_is_final_fragment(p_wsi)) {
			m_recvCS.Enter();
			m_recvQueue.push_back(std::move(m_fragment));
			m_recvCS.Leave();
			m_fragment.clear();
		}
		break;

	case LWS_CALLBACK_CLIENT_WRITEABLE: {
		m_sendCS.Enter();
		if (!m_sendQueue.empty()) {
			std::vector<uint8_t> front = std::move(m_sendQueue.front());
			m_sendQueue.pop_front();
			bool hasMore = !m_sendQueue.empty();
			m_sendCS.Leave();

			size_t payloadLen = front.size() - LWS_PRE;
			lws_write(p_wsi, &front[LWS_PRE], payloadLen, LWS_WRITE_BINARY);

			if (hasMore) {
				lws_callback_on_writable(p_wsi);
			}
		}
		else {
			m_sendCS.Leave();
		}
		break;
	}

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		SDL_Log("[Multiplayer] WebSocket connection error: %s", p_in ? static_cast<const char*>(p_in) : "unknown");
		m_disconnected.store(true);
		m_connected.store(false);
		m_wsi.store(nullptr);
		break;

	case LWS_CALLBACK_CLIENT_CLOSED:
		SDL_Log("[Multiplayer] WebSocket connection closed");
		m_disconnected.store(true);
		m_connected.store(false);
		m_wsi.store(nullptr);
		break;

	default:
		break;
	}

	return 0;
}

} // namespace Multiplayer

#endif // !__EMSCRIPTEN__
