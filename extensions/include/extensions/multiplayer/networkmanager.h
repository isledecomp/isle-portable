#pragma once

#include "extensions/multiplayer/networktransport.h"
#include "extensions/multiplayer/platformcallbacks.h"
#include "extensions/multiplayer/protocol.h"
#include "extensions/multiplayer/remoteplayer.h"
#include "extensions/multiplayer/thirdpersoncamera.h"
#include "extensions/multiplayer/worldstatesync.h"
#include "mxcore.h"
#include "mxtypes.h"

#include <atomic>
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
		return !SDL_strcmp(p_name, NetworkManager::ClassName()) || MxCore::IsA(p_name);
	}

	void Initialize(NetworkTransport* p_transport, PlatformCallbacks* p_callbacks);
	void HandleCreate();
	void Shutdown();

	void Connect(const char* p_roomId);
	void Disconnect();
	bool IsConnected() const;
	bool WasRejected() const;

	void SetWalkAnimation(uint8_t p_walkAnimId);
	void SetIdleAnimation(uint8_t p_idleAnimId);
	void SendEmote(uint8_t p_emoteId);
	void SetDisplayActorIndex(uint8_t p_displayActorIndex);

	// Thread-safe request methods for cross-thread callers (e.g. WASM exports
	// running on the browser main thread).  Deferred to the game thread in Tickle().
	void RequestToggleThirdPerson() { m_pendingToggleThirdPerson.store(true, std::memory_order_relaxed); }
	void RequestSetWalkAnimation(uint8_t p_walkAnimId)
	{
		m_pendingWalkAnim.store(p_walkAnimId, std::memory_order_relaxed);
	}
	void RequestSetIdleAnimation(uint8_t p_idleAnimId)
	{
		m_pendingIdleAnim.store(p_idleAnimId, std::memory_order_relaxed);
	}
	void RequestSendEmote(uint8_t p_emoteId) { m_pendingEmote.store(p_emoteId, std::memory_order_relaxed); }
	void RequestToggleNameBubbles() { m_pendingToggleNameBubbles.store(true, std::memory_order_relaxed); }
	void RequestToggleAllowCustomize() { m_pendingToggleAllowCustomize.store(true, std::memory_order_relaxed); }

	bool IsInIsleWorld() const { return m_inIsleWorld; }
	bool GetShowNameBubbles() const { return m_showNameBubbles; }

	RemotePlayer* FindPlayerByROI(LegoROI* roi) const;
	bool IsClonedCharacter(const char* p_name) const;
	void SendCustomize(uint32_t p_targetPeerId, uint8_t p_changeType, uint8_t p_partIndex);

	void OnWorldEnabled(LegoWorld* p_world);
	void OnWorldDisabled(LegoWorld* p_world);
	void OnBeforeSaveLoad();
	void OnSaveLoaded();

	ThirdPersonCamera& GetThirdPersonCamera() { return m_thirdPersonCamera; }

	void NotifyThirdPersonChanged(bool p_enabled);
	void NotifyNameBubblesChanged(bool p_enabled);
	void NotifyAllowCustomizeChanged(bool p_enabled);

	// Called from multiplayer extension when a plant/building entity is clicked.
	// Returns TRUE if the mutation should be suppressed locally (non-host).
	MxBool HandleEntityMutation(LegoEntity* p_entity, MxU8 p_changeType);

	// Called from multiplayer extension when a sky/light control is used.
	// Returns TRUE if the local action should be suppressed (non-host).
	MxBool HandleSkyLightMutation(uint8_t p_entityType, uint8_t p_changeType);

	bool IsHost() const { return m_localPeerId != 0 && m_localPeerId == m_hostPeerId; }
	uint32_t GetLocalPeerId() const { return m_localPeerId; }

private:
	void BroadcastLocalState();
	void ProcessIncomingPackets();
	void UpdateRemotePlayers(float p_deltaTime);

	RemotePlayer* CreateAndSpawnPlayer(uint32_t p_peerId, uint8_t p_actorId, uint8_t p_displayActorIndex);

	void HandleLeave(const PlayerLeaveMsg& p_msg);
	void HandleState(const PlayerStateMsg& p_msg);
	void HandleHostAssign(const HostAssignMsg& p_msg);
	void HandleEmote(const EmoteMsg& p_msg);
	void HandleCustomize(const CustomizeMsg& p_msg);

	void DeriveDisplayActorIndex(uint8_t p_actorId);
	void ProcessPendingRequests();
	void RemoveRemotePlayer(uint32_t p_peerId);
	void RemoveAllRemotePlayers();

	void NotifyPlayerCountChanged();

	// Serialize and send a fixed-size message via the transport
	template <typename T>
	void SendMessage(const T& p_msg);

	NetworkTransport* m_transport;
	PlatformCallbacks* m_callbacks;
	WorldStateSync m_worldSync;
	ThirdPersonCamera m_thirdPersonCamera;
	std::map<uint32_t, std::unique_ptr<RemotePlayer>> m_remotePlayers;
	std::map<LegoROI*, RemotePlayer*> m_roiToPlayer;

	uint32_t m_localPeerId;
	uint32_t m_hostPeerId;
	uint32_t m_sequence;
	uint32_t m_lastBroadcastTime;
	uint8_t m_lastValidActorId;
	uint8_t m_localWalkAnimId;
	uint8_t m_localIdleAnimId;
	uint8_t m_localDisplayActorIndex;
	bool m_displayActorFrozen;
	bool m_localAllowRemoteCustomize;
	bool m_inIsleWorld;
	bool m_registered;

	std::atomic<bool> m_pendingToggleThirdPerson;
	std::atomic<bool> m_pendingToggleNameBubbles;
	std::atomic<int> m_pendingWalkAnim;
	std::atomic<int> m_pendingIdleAnim;
	std::atomic<int> m_pendingEmote;
	std::atomic<bool> m_pendingToggleAllowCustomize;

	bool m_showNameBubbles;

	static const uint32_t BROADCAST_INTERVAL_MS = 66; // ~15Hz
	static const uint32_t TIMEOUT_MS = 5000;          // 5 second timeout
	static const int EXIT_ROOM_FULL = 10;
};

} // namespace Multiplayer
