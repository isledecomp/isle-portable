// Protocol constants — must stay in sync with protocol.h

// MessageHeader binary layout: type(1) + peerId(4) + sequence(4) = 9 bytes
export const HEADER_SIZE = 9;
export const PEER_ID_OFFSET = 1;
export const SEQUENCE_OFFSET = 5;

// Message types the relay inspects for routing decisions.
// All other types are broadcast to every peer in the room.
export const MSG_LEAVE = 2;
export const MSG_HOST_ASSIGN = 4;
export const MSG_REQUEST_SNAPSHOT = 5;
export const MSG_WORLD_SNAPSHOT = 6;
export const MSG_WORLD_EVENT_REQUEST = 8;
export const MSG_ASSIGN_ID = 0xff;

// AssignIdMsg: compact server-only message — type(1) + peerId(4)
const ASSIGN_ID_SIZE = 1 + 4;

// HostAssignMsg: header(9) + hostPeerId(4)
const HOST_ASSIGN_SIZE = HEADER_SIZE + 4;

// WorldSnapshotMsg: header(9) + targetPeerId(4) + dataLength(2) + data...
export const SNAPSHOT_TARGET_OFFSET = HEADER_SIZE;
export const SNAPSHOT_MIN_SIZE = HEADER_SIZE + 4 + 2;

export function createAssignIdMsg(peerId: number): ArrayBuffer {
	const buf = new ArrayBuffer(ASSIGN_ID_SIZE);
	const view = new DataView(buf);
	view.setUint8(0, MSG_ASSIGN_ID);
	view.setUint32(1, peerId, true);
	return buf;
}

export function createHostAssignMsg(hostPeerId: number): ArrayBuffer {
	const buf = new ArrayBuffer(HOST_ASSIGN_SIZE);
	const view = new DataView(buf);
	view.setUint8(0, MSG_HOST_ASSIGN);
	view.setUint32(PEER_ID_OFFSET, 0, true);
	view.setUint32(SEQUENCE_OFFSET, 0, true);
	view.setUint32(HEADER_SIZE, hostPeerId, true);
	return buf;
}

export function createLeaveMsg(peerId: number): ArrayBuffer {
	const buf = new ArrayBuffer(HEADER_SIZE);
	const view = new DataView(buf);
	view.setUint8(0, MSG_LEAVE);
	view.setUint32(PEER_ID_OFFSET, peerId, true);
	view.setUint32(SEQUENCE_OFFSET, 0, true);
	return buf;
}

/** Copy a message and stamp the sender's peerId into the header. */
export function stampSender(data: Uint8Array, peerId: number): Uint8Array {
	const stamped = new Uint8Array(data.length);
	stamped.set(data);
	new DataView(stamped.buffer).setUint32(PEER_ID_OFFSET, peerId, true);
	return stamped;
}

export function readTargetPeerId(data: Uint8Array): number {
	return new DataView(data.buffer).getUint32(SNAPSHOT_TARGET_OFFSET, true);
}
