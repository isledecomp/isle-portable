export interface Env {
	GAME_ROOM: DurableObjectNamespace;
}

export default {
	async fetch(request: Request, env: Env): Promise<Response> {
		const url = new URL(request.url);
		const pathParts = url.pathname.split("/").filter(Boolean);

		// Route: /room/:roomId
		if (pathParts.length === 2 && pathParts[0] === "room") {
			const roomId = pathParts[1];
			const id = env.GAME_ROOM.idFromName(roomId);
			const room = env.GAME_ROOM.get(id);
			return room.fetch(request);
		}

		// Health check
		if (url.pathname === "/" || url.pathname === "/health") {
			return new Response(JSON.stringify({ status: "ok" }), {
				headers: { "Content-Type": "application/json" },
			});
		}

		return new Response("Not Found", { status: 404 });
	},
};

export class GameRoom implements DurableObject {
	private connections: Map<string, WebSocket> = new Map();
	private nextPeerId: number = 1;

	constructor(
		private state: DurableObjectState,
		private env: Env
	) {}

	async fetch(request: Request): Promise<Response> {
		if (request.headers.get("Upgrade") !== "websocket") {
			return new Response("Expected WebSocket", { status: 426 });
		}

		const pair = new WebSocketPair();
		const [client, server] = [pair[0], pair[1]];

		const peerId = this.nextPeerId++;
		const peerIdStr = String(peerId);

		this.state.acceptWebSocket(server);
		this.connections.set(peerIdStr, server);

		// Send the peer its assigned ID as the first message
		const idMsg = new ArrayBuffer(5);
		const view = new DataView(idMsg);
		view.setUint8(0, 0xff); // Special "assign ID" message type
		view.setUint32(1, peerId, true); // little-endian peer ID
		server.send(idMsg);

		server.addEventListener("message", (event) => {
			if (!(event.data instanceof ArrayBuffer)) {
				return;
			}

			const data = new Uint8Array(event.data);
			if (data.length < 9) {
				return; // Too short for header
			}

			// Stamp the peerId into the message header (bytes 1-4)
			const stamped = new Uint8Array(data.length);
			stamped.set(data);
			new DataView(stamped.buffer).setUint32(1, peerId, true);

			// Broadcast to all other peers in this room
			for (const [id, ws] of this.connections) {
				if (id !== peerIdStr) {
					try {
						ws.send(stamped.buffer);
					} catch {
						this.connections.delete(id);
					}
				}
			}
		});

		server.addEventListener("close", () => {
			this.connections.delete(peerIdStr);

			// Broadcast LEAVE message to remaining peers
			const leaveMsg = new ArrayBuffer(9);
			const leaveView = new DataView(leaveMsg);
			leaveView.setUint8(0, 2); // MSG_LEAVE
			leaveView.setUint32(1, peerId, true);
			leaveView.setUint32(5, 0, true); // sequence 0

			for (const [, ws] of this.connections) {
				try {
					ws.send(leaveMsg);
				} catch {
					// Ignore send errors on cleanup
				}
			}
		});

		server.addEventListener("error", () => {
			this.connections.delete(peerIdStr);
		});

		return new Response(null, { status: 101, webSocket: client });
	}
}
