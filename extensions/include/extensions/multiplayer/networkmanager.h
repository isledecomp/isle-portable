#pragma once

#include "extensions/multiplayer/networktransport.h"
#include "extensions/multiplayer/protocol.h"
#include "extensions/multiplayer/remoteplayer.h"
#include "mxcore.h"
#include "mxtypes.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>

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

	// Called by the Multiplayer extension on world transitions
	void OnWorldEnabled(LegoWorld* p_world);
	void OnWorldDisabled(LegoWorld* p_world);

private:
	void BroadcastLocalState();
	void ProcessIncomingPackets();
	void UpdateRemotePlayers(float p_deltaTime);

	void HandleJoin(const PlayerJoinMsg& p_msg);
	void HandleLeave(const PlayerLeaveMsg& p_msg);
	void HandleState(const PlayerStateMsg& p_msg);

	void RemoveRemotePlayer(uint32_t p_peerId);
	void RemoveAllRemotePlayers();

	int8_t DetectLocalVehicleType();
	bool IsInIsleWorld() const;

	NetworkTransport* m_transport;
	std::map<uint32_t, std::unique_ptr<RemotePlayer>> m_remotePlayers;

	uint32_t m_localPeerId;
	uint32_t m_sequence;
	uint32_t m_lastBroadcastTime;
	uint8_t m_lastValidActorId;
	bool m_inIsleWorld;
	bool m_registered;

	static const uint32_t BROADCAST_INTERVAL_MS = 66; // ~15Hz
	static const uint32_t TIMEOUT_MS = 5000;          // 5 second timeout
};

} // namespace Multiplayer
