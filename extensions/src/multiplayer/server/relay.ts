export { GameRoom } from "./gameroom";

export interface Env {
	GAME_ROOM: DurableObjectNamespace;
	MAX_PLAYERS_CEILING?: number;
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
