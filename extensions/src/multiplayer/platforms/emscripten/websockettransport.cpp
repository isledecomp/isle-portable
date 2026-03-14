#ifdef __EMSCRIPTEN__

#include "extensions/multiplayer/platforms/emscripten/websockettransport.h"

#include <SDL3/SDL_stdinc.h>
#include <emscripten.h>

namespace Multiplayer
{

WebSocketTransport::WebSocketTransport(const std::string& p_relayBaseUrl)
	: m_relayBaseUrl(p_relayBaseUrl), m_socketId(-1), m_connectedFlag(0), m_disconnectedFlag(0), m_wasEverConnected(0)
{
	// clang-format off
	MAIN_THREAD_EM_ASM({
		if (!Module._mpSockets) {
			Module._mpSockets = {};
			Module._mpNextSocketId = 1;
			Module._mpMessageQueues = {};
		}
	});
	// clang-format on
}

WebSocketTransport::~WebSocketTransport()
{
	Disconnect();
}

void WebSocketTransport::Connect(const char* p_roomId)
{
	if (m_connectedFlag) {
		Disconnect();
	}

	std::string url = m_relayBaseUrl + "/room/" + p_roomId;

	m_disconnectedFlag = 0;
	m_wasEverConnected = 0;

	// clang-format off
	m_socketId = MAIN_THREAD_EM_ASM_INT({
		var url = UTF8ToString($0);
		var connPtr = $1;
		var discPtr = $2;
		var everConnPtr = $3;
		var exitRoomFull = $4;
		var exitConnLost = $5;
		var socketId = Module._mpNextSocketId++;
		Module._mpMessageQueues[socketId] = [];

		try {
			var ws = new WebSocket(url);
			ws.binaryType = 'arraybuffer';

			ws.onopen = function() {
				Atomics.store(HEAP32, connPtr >> 2, 1);
				Atomics.store(HEAP32, everConnPtr >> 2, 1);
			};

			ws.onmessage = function(event) {
				if (event.data instanceof ArrayBuffer) {
					var data = new Uint8Array(event.data);
					Module._mpMessageQueues[socketId].push(data);
				}
			};

			ws.onclose = function() {
				var wasEverConnected = Atomics.load(HEAP32, everConnPtr >> 2);
				Atomics.store(HEAP32, connPtr >> 2, 0);
				Atomics.store(HEAP32, discPtr >> 2, 1);
				Module._exitCode = wasEverConnected ? exitConnLost : exitRoomFull;
			};

			ws.onerror = function() {
				Atomics.store(HEAP32, connPtr >> 2, 0);
			};

			Module._mpSockets[socketId] = ws;
		} catch (e) {
			console.error('WebSocket connect error:', e);
			return -1;
		}

		return socketId;
	}, url.c_str(), &m_connectedFlag, &m_disconnectedFlag, &m_wasEverConnected, EXIT_ROOM_FULL, EXIT_CONNECTION_LOST);
	// clang-format on

	if (m_socketId <= 0) {
		m_socketId = -1;
	}
}

void WebSocketTransport::Disconnect()
{
	if (m_socketId > 0) {
		// clang-format off
		MAIN_THREAD_EM_ASM({
			var socketId = $0;
			if (Module._mpSockets[socketId]) {
				Module._mpSockets[socketId].close();
				delete Module._mpSockets[socketId];
			}
			delete Module._mpMessageQueues[socketId];
		}, m_socketId);
		// clang-format on

		m_socketId = -1;
		m_connectedFlag = 0;
		m_wasEverConnected = 0;
	}
}

bool WebSocketTransport::IsConnected() const
{
	return m_socketId > 0 && m_connectedFlag != 0;
}

bool WebSocketTransport::WasDisconnected() const
{
	return m_disconnectedFlag != 0;
}

bool WebSocketTransport::WasRejected() const
{
	return m_disconnectedFlag != 0 && m_wasEverConnected == 0;
}

void WebSocketTransport::Send(const uint8_t* p_data, size_t p_length)
{
	if (m_socketId <= 0 || !m_connectedFlag) {
		return;
	}

	// clang-format off
	MAIN_THREAD_EM_ASM({
		var socketId = $0;
		var dataPtr = $1;
		var length = $2;
		var ws = Module._mpSockets[socketId];
		if (ws && ws.readyState === WebSocket.OPEN) {
			var buffer = new Uint8Array(HEAPU8.buffer, dataPtr, length);
			var copy = new Uint8Array(length);
			copy.set(buffer);
			ws.send(copy.buffer);
		}
	}, m_socketId, p_data, (int) p_length);
	// clang-format on
}

size_t WebSocketTransport::Receive(std::function<void(const uint8_t*, size_t)> p_callback)
{
	if (m_socketId <= 0) {
		return 0;
	}

	// Drain queued messages in one proxy call: [4-byte LE length][payload...] each.
	// clang-format off
	int totalBytes = MAIN_THREAD_EM_ASM_INT({
		var socketId = $0;
		var destPtr = $1;
		var maxBytes = $2;
		var queue = Module._mpMessageQueues[socketId];
		if (!queue || queue.length === 0) {
			return 0;
		}
		var offset = 0;
		var view = new DataView(HEAPU8.buffer);
		while (queue.length > 0) {
			var msg = queue[0];
			var needed = 4 + msg.length;
			if (offset + needed > maxBytes) {
				break;
			}
			view.setUint32(destPtr + offset, msg.length, true);
			offset += 4;
			HEAPU8.set(msg, destPtr + offset);
			offset += msg.length;
			queue.shift();
		}
		return offset;
	}, m_socketId, m_recvBuf, (int) sizeof(m_recvBuf));
	// clang-format on

	if (totalBytes <= 0) {
		return 0;
	}

	size_t processed = 0;
	int offset = 0;

	while (offset + 4 <= totalBytes) {
		uint32_t msgLen;
		SDL_memcpy(&msgLen, m_recvBuf + offset, sizeof(uint32_t));
		offset += 4;

		if (msgLen == 0 || offset + (int) msgLen > totalBytes) {
			break;
		}

		p_callback(m_recvBuf + offset, (size_t) msgLen);
		offset += msgLen;
		processed++;
	}

	return processed;
}

} // namespace Multiplayer

#endif // __EMSCRIPTEN__
