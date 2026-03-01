import {
	HEADER_SIZE,
	MSG_REQUEST_SNAPSHOT,
	MSG_WORLD_EVENT_REQUEST,
	MSG_WORLD_SNAPSHOT,
	SNAPSHOT_MIN_SIZE,
	createAssignIdMsg,
	createHostAssignMsg,
	createLeaveMsg,
	readTargetPeerId,
	stampSender,
} from "./protocol";
import type { Env } from "./relay";

export class GameRoom implements DurableObject {
	private connections = new Map<number, WebSocket>();
	private nextPeerId = 1;
	private hostPeerId = 0;

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

		server.accept();
		this.connections.set(peerId, server);

		server.send(createAssignIdMsg(peerId));
		this.assignHostIfNeeded(peerId, server);

		server.addEventListener("message", (event) =>
			this.handleMessage(event, peerId)
		);

		const handleDisconnect = () => this.handleDisconnect(peerId);
		server.addEventListener("close", handleDisconnect);
		server.addEventListener("error", handleDisconnect);

		return new Response(null, { status: 101, webSocket: client });
	}

	// ---- Connection lifecycle ----

	private assignHostIfNeeded(peerId: number, ws: WebSocket): void {
		if (this.hostPeerId === 0 || !this.connections.has(this.hostPeerId)) {
			this.hostPeerId = peerId;
			this.broadcast(createHostAssignMsg(this.hostPeerId));
		} else {
			this.trySend(ws, createHostAssignMsg(this.hostPeerId));
		}
	}

	private handleDisconnect(peerId: number): void {
		this.connections.delete(peerId);
		this.broadcast(createLeaveMsg(peerId));

		if (peerId === this.hostPeerId) {
			this.electNewHost();
		}
	}

	// ---- Message routing ----

	private handleMessage(event: MessageEvent, peerId: number): void {
		if (!(event.data instanceof ArrayBuffer)) {
			return;
		}

		const data = new Uint8Array(event.data);
		if (data.length < HEADER_SIZE) {
			return;
		}

		const msgType = data[0];
		const stamped = stampSender(data, peerId);

		if (
			msgType === MSG_REQUEST_SNAPSHOT ||
			msgType === MSG_WORLD_EVENT_REQUEST
		) {
			this.sendToHost(stamped);
		} else if (
			msgType === MSG_WORLD_SNAPSHOT &&
			data.length >= SNAPSHOT_MIN_SIZE
		) {
			this.sendToTarget(stamped);
		} else {
			this.broadcastExcept(stamped.buffer, peerId);
		}
	}

	private sendToHost(data: Uint8Array): void {
		const hostWs = this.connections.get(this.hostPeerId);
		if (hostWs) {
			this.trySend(hostWs, data.buffer);
		}
	}

	private sendToTarget(data: Uint8Array): void {
		const targetId = readTargetPeerId(data);
		const targetWs = this.connections.get(targetId);
		if (targetWs) {
			if (!this.trySend(targetWs, data.buffer)) {
				this.connections.delete(targetId);
			}
		}
	}

	// ---- Broadcasting ----

	private broadcast(msg: ArrayBuffer): void {
		for (const ws of this.connections.values()) {
			this.trySend(ws, msg);
		}
	}

	private broadcastExcept(msg: ArrayBuffer, excludePeerId: number): void {
		for (const [id, ws] of this.connections) {
			if (id !== excludePeerId) {
				if (!this.trySend(ws, msg)) {
					this.connections.delete(id);
				}
			}
		}
	}

	private trySend(ws: WebSocket, data: ArrayBuffer): boolean {
		try {
			ws.send(data);
			return true;
		} catch {
			return false;
		}
	}

	// ---- Host election ----

	private electNewHost(): void {
		this.hostPeerId = 0;

		for (const id of this.connections.keys()) {
			if (this.hostPeerId === 0 || id < this.hostPeerId) {
				this.hostPeerId = id;
			}
		}

		if (this.hostPeerId > 0) {
			this.broadcast(createHostAssignMsg(this.hostPeerId));
		}
	}
}
