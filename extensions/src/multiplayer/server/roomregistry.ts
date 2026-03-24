import type { Env } from "./relay";
import { CORS_HEADERS } from "./cors";

const STALE_THRESHOLD_MS = 5 * 60 * 1000; // 5 minutes
const ALARM_INTERVAL_MS = 60 * 1000; // 60 seconds
const MAX_LISTED_ROOMS = 5;

interface RoomEntry {
	roomId: string;
	players: number;
	maxPlayers: number;
	createdAt: number;
	updatedAt: number;
}

export class RoomRegistry implements DurableObject {
	constructor(
		private state: DurableObjectState,
		private env: Env
	) {}

	async fetch(request: Request): Promise<Response> {
		const method = request.method.toUpperCase();

		if (method === "GET") {
			return this.handleList();
		}

		if (method === "POST") {
			return this.handleUpsert(request);
		}

		return new Response("Method Not Allowed", {
			status: 405,
			headers: CORS_HEADERS,
		});
	}

	private async handleList(): Promise<Response> {
		const entries = await this.getAllRooms();

		entries.sort((a, b) => {
			if (b.players !== a.players) return b.players - a.players;
			return b.createdAt - a.createdAt;
		});

		const rooms = entries.slice(0, MAX_LISTED_ROOMS).map((e) => ({
			roomId: e.roomId,
			players: e.players,
			maxPlayers: e.maxPlayers,
		}));

		return new Response(JSON.stringify({ rooms }), {
			headers: {
				"Content-Type": "application/json",
				...CORS_HEADERS,
			},
		});
	}

	private async handleUpsert(request: Request): Promise<Response> {
		try {
			const body = (await request.json()) as {
				roomId: string;
				players: number;
				maxPlayers: number;
			};

			const key = `room:${body.roomId}`;
			const existing =
				await this.state.storage.get<RoomEntry>(key);
			const now = Date.now();

			const entry: RoomEntry = {
				roomId: body.roomId,
				players: body.players,
				maxPlayers: body.maxPlayers,
				createdAt: existing?.createdAt ?? now,
				updatedAt: now,
			};

			await this.state.storage.put(key, entry);
			await this.ensureAlarm();

			return new Response(JSON.stringify({ ok: true }), {
				headers: {
					"Content-Type": "application/json",
					...CORS_HEADERS,
				},
			});
		} catch {
			return new Response("Bad Request", {
				status: 400,
				headers: CORS_HEADERS,
			});
		}
	}

	async alarm(): Promise<void> {
		const entries = await this.getAllRooms();
		const now = Date.now();
		const staleKeys: string[] = [];

		for (const entry of entries) {
			if (now - entry.updatedAt > STALE_THRESHOLD_MS) {
				staleKeys.push(`room:${entry.roomId}`);
			}
		}

		if (staleKeys.length > 0) {
			await this.state.storage.delete(staleKeys);
		}

		const remaining = entries.length - staleKeys.length;
		if (remaining > 0) {
			await this.state.storage.setAlarm(
				Date.now() + ALARM_INTERVAL_MS
			);
		}
	}

	private async getAllRooms(): Promise<RoomEntry[]> {
		const map = await this.state.storage.list<RoomEntry>({
			prefix: "room:",
		});
		return [...map.values()];
	}

	private async ensureAlarm(): Promise<void> {
		const currentAlarm = await this.state.storage.getAlarm();
		if (currentAlarm === null) {
			await this.state.storage.setAlarm(
				Date.now() + ALARM_INTERVAL_MS
			);
		}
	}
}
