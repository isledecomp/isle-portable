#pragma once

#include "extensions/multiplayer/networktransport.h"
#include "extensions/multiplayer/protocol.h"
#include "mxtypes.h"

#include <cstdint>
#include <vector>

class LegoEntity;

namespace Multiplayer
{

class WorldStateSync {
public:
	WorldStateSync();

	void SetTransport(NetworkTransport* p_transport) { m_transport = p_transport; }
	void SetLocalPeerId(uint32_t p_peerId) { m_localPeerId = p_peerId; }
	void SetHost(bool p_isHost) { m_isHost = p_isHost; }
	void SetInIsleWorld(bool p_inIsle) { m_inIsleWorld = p_inIsle; }

	// Called when the host peer changes. Requests a snapshot if we're not host.
	void OnHostChanged();

	// Incoming message handlers (called from NetworkManager::ProcessIncomingPackets)
	void HandleRequestSnapshot(const RequestSnapshotMsg& p_msg);
	void HandleWorldSnapshot(const uint8_t* p_data, size_t p_length);
	void HandleWorldEvent(const WorldEventMsg& p_msg);
	void HandleWorldEventRequest(const WorldEventRequestMsg& p_msg);

	// Called from multiplayer extension when a plant/building entity is clicked.
	// Returns TRUE if the mutation should be suppressed locally (non-host).
	MxBool HandleEntityMutation(LegoEntity* p_entity, MxU8 p_changeType);

private:
	void SendSnapshotRequest();
	void SendWorldSnapshot(uint32_t p_targetPeerId);
	void BroadcastWorldEvent(uint8_t p_entityType, uint8_t p_changeType, uint8_t p_entityIndex);
	void SendWorldEventRequest(uint8_t p_entityType, uint8_t p_changeType, uint8_t p_entityIndex);
	void ApplyWorldEvent(uint8_t p_entityType, uint8_t p_changeType, uint8_t p_entityIndex);

	template <typename T>
	void SendMessage(const T& p_msg);

	NetworkTransport* m_transport;
	uint32_t m_localPeerId;
	uint32_t m_sequence;
	bool m_isHost;
	bool m_inIsleWorld;
	bool m_snapshotRequested;
	std::vector<WorldEventMsg> m_pendingWorldEvents;
};

} // namespace Multiplayer
