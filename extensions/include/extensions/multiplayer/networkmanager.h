#pragma once

#include "extensions/multiplayer/networktransport.h"
#include "extensions/multiplayer/protocol.h"
#include "extensions/multiplayer/remoteplayer.h"
#include "extensions/multiplayer/worldstatesync.h"
#include "mxcore.h"
#include "mxtypes.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

class LegoEntity;
class LegoWorld;

namespace Multiplayer
{

class NetworkManager : public MxCore {
public:
	NetworkManager();
	~NetworkManager() override;

	MxResult Tickle() override;

	const char* ClassName() const override { return "NetworkManager"; }

	MxBool IsA(const char* p_name) const override
	{
		return !strcmp(p_name, NetworkManager::ClassName()) || MxCore::IsA(p_name);
	}

	void Initialize(NetworkTransport* p_transport);
	void Shutdown();

	void Connect(const char* p_roomId);
	void Disconnect();
	bool IsConnected() const;
	bool WasRejected() const;

	void OnWorldEnabled(LegoWorld* p_world);
	void OnWorldDisabled(LegoWorld* p_world);

	// Called from multiplayer extension when a plant/building entity is clicked.
	// Returns TRUE if the mutation should be suppressed locally (non-host).
	MxBool HandleEntityMutation(LegoEntity* p_entity, MxU8 p_changeType);

	bool IsHost() const { return m_localPeerId != 0 && m_localPeerId == m_hostPeerId; }

private:
	void BroadcastLocalState();
	void ProcessIncomingPackets();
	void UpdateRemotePlayers(float p_deltaTime);

	RemotePlayer* CreateAndSpawnPlayer(uint32_t p_peerId, uint8_t p_actorId);

	void HandleJoin(const PlayerJoinMsg& p_msg);
	void HandleLeave(const PlayerLeaveMsg& p_msg);
	void HandleState(const PlayerStateMsg& p_msg);
	void HandleHostAssign(const HostAssignMsg& p_msg);

	void RemoveRemotePlayer(uint32_t p_peerId);
	void RemoveAllRemotePlayers();

	int8_t DetectLocalVehicleType();

	// Serialize and send a fixed-size message via the transport
	template <typename T>
	void SendMessage(const T& p_msg);

	NetworkTransport* m_transport;
	WorldStateSync m_worldSync;
	std::map<uint32_t, std::unique_ptr<RemotePlayer>> m_remotePlayers;

	uint32_t m_localPeerId;
	uint32_t m_hostPeerId;
	uint32_t m_sequence;
	uint32_t m_lastBroadcastTime;
	uint8_t m_lastValidActorId;
	bool m_inIsleWorld;
	bool m_registered;

	static const uint32_t BROADCAST_INTERVAL_MS = 66; // ~15Hz
	static const uint32_t TIMEOUT_MS = 5000;          // 5 second timeout
	static const int EXIT_ROOM_FULL = 10;
};

} // namespace Multiplayer
