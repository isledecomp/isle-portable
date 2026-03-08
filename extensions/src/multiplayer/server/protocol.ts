// Protocol constants — must stay in sync with protocol.h

// MessageHeader binary layout: type(1) + peerId(4) + sequence(4) + target(4) = 13 bytes
export const HEADER_SIZE = 13;
export const PEER_ID_OFFSET = 1;
export const SEQUENCE_OFFSET = 5;
export const TARGET_OFFSET = 9;

// Routing target constants
export const TARGET_BROADCAST = 0;
export const TARGET_HOST = 0xffffffff;
export const TARGET_BROADCAST_ALL = 0xfffffffe;

// Message types used by server message constructors only.
export const MSG_LEAVE = 2;
export const MSG_HOST_ASSIGN = 4;
export const MSG_ASSIGN_ID = 0xff;

// AssignIdMsg: compact server-only message — type(1) + peerId(4) + maxActors(1)
const ASSIGN_ID_SIZE = 1 + 4 + 1;

// HostAssignMsg: header(13) + hostPeerId(4)
const HOST_ASSIGN_SIZE = HEADER_SIZE + 4;

export function createAssignIdMsg(peerId: number, maxActors: number): ArrayBuffer {
	const buf = new ArrayBuffer(ASSIGN_ID_SIZE);
	const view = new DataView(buf);
	view.setUint8(0, MSG_ASSIGN_ID);
	view.setUint32(1, peerId, true);
	view.setUint8(5, maxActors);
	return buf;
}

export function createHostAssignMsg(hostPeerId: number): ArrayBuffer {
	const buf = new ArrayBuffer(HOST_ASSIGN_SIZE);
	const view = new DataView(buf);
	view.setUint8(0, MSG_HOST_ASSIGN);
	view.setUint32(PEER_ID_OFFSET, 0, true);
	view.setUint32(SEQUENCE_OFFSET, 0, true);
	view.setUint32(TARGET_OFFSET, 0, true);
	view.setUint32(HEADER_SIZE, hostPeerId, true);
	return buf;
}

export function createLeaveMsg(peerId: number): ArrayBuffer {
	const buf = new ArrayBuffer(HEADER_SIZE);
	const view = new DataView(buf);
	view.setUint8(0, MSG_LEAVE);
	view.setUint32(PEER_ID_OFFSET, peerId, true);
	view.setUint32(SEQUENCE_OFFSET, 0, true);
	view.setUint32(TARGET_OFFSET, 0, true);
	return buf;
}

/** Copy a message and stamp the sender's peerId into the header. */
export function stampSender(data: Uint8Array, peerId: number): Uint8Array {
	const stamped = new Uint8Array(data.length);
	stamped.set(data);
	new DataView(stamped.buffer).setUint32(PEER_ID_OFFSET, peerId, true);
	return stamped;
}

export function readTarget(data: Uint8Array): number {
	return new DataView(data.buffer).getUint32(TARGET_OFFSET, true);
}
