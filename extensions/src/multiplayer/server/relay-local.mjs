import { createServer } from "http";
import { WebSocketServer } from "ws";

const PORT = process.env.PORT || 8787;
const rooms = new Map();

function getRoom(roomId) {
	if (!rooms.has(roomId)) {
		rooms.set(roomId, { connections: new Map(), nextPeerId: 1 });
	}
	return rooms.get(roomId);
}

const server = createServer((req, res) => {
	if (req.url === "/" || req.url === "/health") {
		res.writeHead(200, { "Content-Type": "application/json" });
		res.end(JSON.stringify({ status: "ok" }));
	} else {
		res.writeHead(404);
		res.end("Not Found");
	}
});

const wss = new WebSocketServer({ server });

wss.on("connection", (ws, req) => {
	const pathParts = (req.url || "").split("/").filter(Boolean);
	if (pathParts.length !== 2 || pathParts[0] !== "room") {
		console.log(`[REJECT] Invalid path: ${req.url}`);
		ws.close(1008, "Invalid path");
		return;
	}

	const roomId = pathParts[1];
	const room = getRoom(roomId);
	const peerId = room.nextPeerId++;
	const peerIdStr = String(peerId);
	room.connections.set(peerIdStr, ws);
	console.log(`[CONNECT] Peer ${peerId} joined room "${roomId}" (${room.connections.size} peers)`);

	// Send the peer its assigned ID as the first message
	const idMsg = Buffer.alloc(5);
	idMsg.writeUInt8(0xff, 0);
	idMsg.writeUInt32LE(peerId, 1);
	ws.send(idMsg);

	ws.on("message", (data) => {
		if (!(data instanceof Buffer) || data.length < 9) {
			return;
		}

		const msgType = data.readUInt8(0);
		console.log(`[MSG] Peer ${peerId} sent type=${msgType} len=${data.length}`);

		// Stamp the peerId into the message header (bytes 1-4)
		const stamped = Buffer.from(data);
		stamped.writeUInt32LE(peerId, 1);

		// Broadcast to all other peers in this room
		let sent = 0;
		for (const [id, peer] of room.connections) {
			if (id !== peerIdStr) {
				try {
					peer.send(stamped);
					sent++;
				} catch {
					room.connections.delete(id);
				}
			}
		}
		console.log(`[RELAY] Forwarded to ${sent} peers`);
	});

	const onClose = () => {
		console.log(`[DISCONNECT] Peer ${peerId} left room "${roomId}" (${room.connections.size - 1} peers remaining)`);
		room.connections.delete(peerIdStr);

		// Broadcast LEAVE message to remaining peers
		const leaveMsg = Buffer.alloc(9);
		leaveMsg.writeUInt8(2, 0); // MSG_LEAVE
		leaveMsg.writeUInt32LE(peerId, 1);
		leaveMsg.writeUInt32LE(0, 5); // sequence 0

		for (const [, peer] of room.connections) {
			try {
				peer.send(leaveMsg);
			} catch {
				// Ignore send errors on cleanup
			}
		}

		// Clean up empty rooms
		if (room.connections.size === 0) {
			rooms.delete(pathParts[1]);
		}
	};

	ws.on("close", onClose);
	ws.on("error", onClose);
});

server.listen(PORT, () => {
	console.log(`Relay server listening on http://localhost:${PORT}`);
});
