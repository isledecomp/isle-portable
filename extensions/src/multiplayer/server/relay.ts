export { GameRoom } from "./gameroom";
export { RoomRegistry } from "./roomregistry";

export interface Env {
	GAME_ROOM: DurableObjectNamespace;
	ROOM_REGISTRY: DurableObjectNamespace;
	MAX_PLAYERS_CEILING?: number;
}

import { CORS_HEADERS } from "./cors";

export default {
	async fetch(request: Request, env: Env): Promise<Response> {
		if (request.method === "OPTIONS") {
			return new Response(null, { status: 204, headers: CORS_HEADERS });
		}

		const url = new URL(request.url);
		const pathParts = url.pathname.split("/").filter(Boolean);

		// Route: /rooms (public room listing)
		if (url.pathname === "/rooms" && request.method === "GET") {
			const id = env.ROOM_REGISTRY.idFromName("global");
			const registry = env.ROOM_REGISTRY.get(id);
			return registry.fetch(request);
		}

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
