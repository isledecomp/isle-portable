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

// Message types matching protocol.h
const MSG_LEAVE = 2;
const MSG_HOST_ASSIGN = 4;
const MSG_REQUEST_SNAPSHOT = 5;
const MSG_WORLD_SNAPSHOT = 6;
const MSG_WORLD_EVENT_REQUEST = 8;
const MSG_ASSIGN_ID = 0xff;

export class GameRoom implements DurableObject {
	private connections: Map<string, WebSocket> = new Map();
	private nextPeerId: number = 1;
	private hostPeerId: number = 0;

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

		server.accept();
		this.connections.set(peerIdStr, server);

		// Send the peer its assigned ID as the first message
		const idMsg = new ArrayBuffer(5);
		const view = new DataView(idMsg);
		view.setUint8(0, MSG_ASSIGN_ID);
		view.setUint32(1, peerId, true); // little-endian peer ID
		server.send(idMsg);

		// Assign host if none exists (first peer becomes host)
		if (this.hostPeerId === 0 || !this.connections.has(String(this.hostPeerId))) {
			this.hostPeerId = peerId;
			this.broadcastHostAssign();
		} else {
			// Send current host assignment to the new peer only
			this.sendHostAssign(server);
		}

		server.addEventListener("message", (event) => {
			if (!(event.data instanceof ArrayBuffer)) {
				return;
			}

			const data = new Uint8Array(event.data);
			if (data.length < 9) {
				return; // Too short for header
			}

			const msgType = data[0];

			// Stamp the peerId into the message header (bytes 1-4)
			const stamped = new Uint8Array(data.length);
			stamped.set(data);
			new DataView(stamped.buffer).setUint32(1, peerId, true);

			if (msgType === MSG_REQUEST_SNAPSHOT || msgType === MSG_WORLD_EVENT_REQUEST) {
				// Route to host only
				const hostWs = this.connections.get(String(this.hostPeerId));
				if (hostWs) {
					try {
						hostWs.send(stamped.buffer);
					} catch {
						// Host disconnected; will be handled by close event
					}
				}
			} else if (msgType === MSG_WORLD_SNAPSHOT && data.length >= 15) {
				// Route to the target peer only (targetPeerId at offset 9)
				const targetId = new DataView(stamped.buffer).getUint32(9, true);
				const targetWs = this.connections.get(String(targetId));
				if (targetWs) {
					try {
						targetWs.send(stamped.buffer);
					} catch {
						this.connections.delete(String(targetId));
					}
				}
			} else {
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
			}
		});

		const handleDisconnect = () => {
			this.connections.delete(peerIdStr);

			// Broadcast LEAVE message to remaining peers
			const leaveMsg = new ArrayBuffer(9);
			const leaveView = new DataView(leaveMsg);
			leaveView.setUint8(0, MSG_LEAVE);
			leaveView.setUint32(1, peerId, true);
			leaveView.setUint32(5, 0, true); // sequence 0

			for (const [, ws] of this.connections) {
				try {
					ws.send(leaveMsg);
				} catch {
					// Ignore send errors on cleanup
				}
			}

			// Host migration: if the disconnected peer was the host, assign a new one
			if (peerId === this.hostPeerId) {
				this.electNewHost();
			}
		};

		server.addEventListener("close", handleDisconnect);
		server.addEventListener("error", handleDisconnect);

		return new Response(null, { status: 101, webSocket: client });
	}

	private electNewHost(): void {
		// Pick the lowest peer ID from remaining connections
		let lowestId = 0;
		for (const idStr of this.connections.keys()) {
			const id = parseInt(idStr, 10);
			if (lowestId === 0 || id < lowestId) {
				lowestId = id;
			}
		}

		this.hostPeerId = lowestId;
		if (lowestId > 0) {
			this.broadcastHostAssign();
		}
	}

	private broadcastHostAssign(): void {
		const msg = this.createHostAssignMsg();
		for (const [, ws] of this.connections) {
			try {
				ws.send(msg);
			} catch {
				// Ignore send errors
			}
		}
	}

	private sendHostAssign(ws: WebSocket): void {
		try {
			ws.send(this.createHostAssignMsg());
		} catch {
			// Ignore send errors
		}
	}

	private createHostAssignMsg(): ArrayBuffer {
		// MessageHeader (9 bytes) + hostPeerId (4 bytes) = 13 bytes
		const msg = new ArrayBuffer(13);
		const view = new DataView(msg);
		view.setUint8(0, MSG_HOST_ASSIGN); // type
		view.setUint32(1, 0, true); // peerId (server, so 0)
		view.setUint32(5, 0, true); // sequence
		view.setUint32(9, this.hostPeerId, true); // hostPeerId
		return msg;
	}
}
