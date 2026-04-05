import {
	HEADER_SIZE,
	TARGET_BROADCAST,
	TARGET_HOST,
	TARGET_BROADCAST_ALL,
	createAssignIdMsg,
	createHostAssignMsg,
	createLeaveMsg,
	readTarget,
	stampSender,
} from "./protocol";
import type { Env } from "./relay";
import { CORS_HEADERS } from "./cors";

export class GameRoom implements DurableObject {
	private connections = new Map<number, WebSocket>();
	private nextPeerId = 1;
	private hostPeerId = 0;
	private maxPlayers = 5;
	private isPublic = true;
	private roomId: string | null = null;

	constructor(
		private state: DurableObjectState,
		private env: Env
	) {
		state.blockConcurrencyWhile(async () => {
			this.isPublic =
				(await state.storage.get<boolean>("isPublic")) ?? true;
			this.roomId =
				(await state.storage.get<string>("roomId")) ?? null;
			this.maxPlayers =
				(await state.storage.get<number>("maxPlayers")) ?? 5;
		});
	}

	async fetch(request: Request): Promise<Response> {
		// Extract roomId from URL path if not yet known
		if (!this.roomId) {
			const url = new URL(request.url);
			const parts = url.pathname.split("/").filter(Boolean);
			if (parts.length === 2 && parts[0] === "room") {
				this.roomId = parts[1];
				await this.state.storage.put("roomId", this.roomId);
			}
		}

		// Handle non-WebSocket requests (HTTP API)
		if (request.headers.get("Upgrade") !== "websocket") {
			return this.handleHttpRequest(request);
		}

		// Capacity check
		if (this.connections.size >= this.maxPlayers) {
			return new Response("Room is full", {
				status: 503,
				headers: CORS_HEADERS,
			});
		}

		const pair = new WebSocketPair();
		const [client, server] = [pair[0], pair[1]];

		const peerId = this.nextPeerId++;

		server.accept();
		this.connections.set(peerId, server);
		this.notifyRegistry().catch(() => {});

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

	private async handleHttpRequest(request: Request): Promise<Response> {
		const method = request.method.toUpperCase();

		if (method === "POST") {
			try {
				const body = (await request.json()) as {
					maxPlayers?: number;
					isPublic?: boolean;
				};
				const ceiling = this.env.MAX_PLAYERS_CEILING
					? Number(this.env.MAX_PLAYERS_CEILING)
					: 64;
				if (body.maxPlayers !== undefined) {
					this.maxPlayers = Math.max(
						2,
						Math.min(body.maxPlayers, ceiling)
					);
					await this.state.storage.put(
						"maxPlayers",
						this.maxPlayers
					);
				}
				if (body.isPublic !== undefined) {
					this.isPublic = body.isPublic;
					await this.state.storage.put(
						"isPublic",
						this.isPublic
					);
				}
			} catch {
				// Ignore parse errors, keep defaults
			}
			this.notifyRegistry().catch(() => {});
			return new Response(
				JSON.stringify({ maxPlayers: this.maxPlayers }),
				{
					headers: {
						"Content-Type": "application/json",
						...CORS_HEADERS,
					},
				}
			);
		}

		if (method === "GET") {
			return new Response(
				JSON.stringify({
					players: this.connections.size,
					maxPlayers: this.maxPlayers,
					isPublic: this.isPublic,
				}),
				{
					headers: {
						"Content-Type": "application/json",
						...CORS_HEADERS,
					},
				}
			);
		}

		return new Response("Method Not Allowed", {
			status: 405,
			headers: CORS_HEADERS,
		});
	}

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
		this.notifyRegistry().catch(() => {});

		if (peerId === this.hostPeerId) {
			this.electNewHost();
		}
	}

	private handleMessage(event: MessageEvent, peerId: number): void {
		if (!(event.data instanceof ArrayBuffer)) {
			return;
		}

		const data = new Uint8Array(event.data);
		if (data.length < HEADER_SIZE) {
			return;
		}

		const stamped = stampSender(data, peerId);
		const target = readTarget(stamped);

		if (target === TARGET_BROADCAST) {
			this.broadcastExcept(stamped.buffer, peerId);
		} else if (target === TARGET_HOST) {
			this.sendToHost(stamped);
		} else if (target === TARGET_BROADCAST_ALL) {
			this.broadcast(stamped.buffer);
		} else {
			this.sendToTarget(stamped, target);
		}
	}

	private sendToHost(data: Uint8Array): void {
		const hostWs = this.connections.get(this.hostPeerId);
		if (hostWs) {
			this.trySend(hostWs, data.buffer);
		}
	}

	private sendToTarget(data: Uint8Array, targetId: number): void {
		const targetWs = this.connections.get(targetId);
		if (targetWs) {
			if (!this.trySend(targetWs, data.buffer)) {
				this.connections.delete(targetId);
			}
		}
	}

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

	private async notifyRegistry(): Promise<void> {
		if (!this.isPublic || !this.roomId) return;
		const id = this.env.ROOM_REGISTRY.idFromName("global");
		const registry = this.env.ROOM_REGISTRY.get(id);
		await registry.fetch(
			new Request("https://registry/", {
				method: "POST",
				headers: { "Content-Type": "application/json" },
				body: JSON.stringify({
					roomId: this.roomId,
					players: this.connections.size,
					maxPlayers: this.maxPlayers,
				}),
			})
		);
	}
}
