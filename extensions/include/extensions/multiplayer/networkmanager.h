#pragma once

#include "extensions/multiplayer/animation/catalog.h"
#include "extensions/multiplayer/animation/coordinator.h"
#include "extensions/multiplayer/animation/locationproximity.h"
#include "extensions/multiplayer/animation/sceneplayer.h"
#include "extensions/multiplayer/animation/sessionhost.h"
#include "extensions/multiplayer/networktransport.h"
#include "extensions/multiplayer/platformcallbacks.h"
#include "extensions/multiplayer/protocol.h"
#include "extensions/multiplayer/remoteplayer.h"
#include "extensions/multiplayer/sireader.h"
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

namespace Extensions
{
class ThirdPersonCameraExt;
}

namespace Multiplayer
{

class NameBubbleRenderer;

class NetworkManager : public MxCore {
public:
	enum ConnectionState {
		STATE_DISCONNECTED,
		STATE_CONNECTED,
		STATE_RECONNECTING
	};

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
	void SendHorn(int8_t p_vehicleType);

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
	void RequestSetAnimInterest(int32_t p_animIndex)
	{
		m_pendingAnimInterest.store(p_animIndex, std::memory_order_relaxed);
	}
	void RequestCancelAnimInterest() { m_pendingAnimCancel.store(true, std::memory_order_relaxed); }

	bool IsInIsleWorld() const { return m_inIsleWorld; }

	RemotePlayer* FindPlayerByROI(LegoROI* p_roi) const;
	bool IsClonedCharacter(const char* p_name) const;
	void SendCustomize(uint32_t p_targetPeerId, uint8_t p_changeType, uint8_t p_partIndex);

	// Stop any playing animation and release its resources.
	// Must be called before the display ROI is destroyed.
	void StopAnimation();

	void OnWorldEnabled(LegoWorld* p_world);
	void OnWorldDisabled(LegoWorld* p_world);
	void OnBeforeSaveLoad();
	void OnSaveLoaded();

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
	void HandleHorn(const HornMsg& p_msg);
	void HandleCustomize(const CustomizeMsg& p_msg);

	// Animation coordination handlers
	void HandleAnimInterest(uint32_t p_peerId, uint16_t p_animIndex, uint8_t p_displayActorIndex);
	void HandleAnimCancel(uint32_t p_peerId);
	void HandleAnimUpdate(const AnimUpdateMsg& p_msg);
	void HandleAnimStart(const AnimStartMsg& p_msg);
	void HandleAnimStartLocally(uint16_t p_animIndex, bool p_localInSession, uint64_t p_eventId);
	AnimUpdateMsg BuildAnimUpdateMsg(uint16_t p_animIndex, uint32_t p_target);
	void BroadcastAnimUpdate(uint16_t p_animIndex);
	void SendAnimUpdateToPlayer(uint16_t p_animIndex, uint32_t p_targetPeerId);
	void BroadcastAnimStart(uint16_t p_animIndex, uint64_t p_eventId);
	bool IsPeerAtLocation(uint32_t p_peerId, int16_t p_location) const;
	bool GetPeerPosition(uint32_t p_peerId, float& p_x, float& p_z) const;
	bool IsPeerNearby(uint32_t p_peerId, float p_refX, float p_refZ) const;
	uint8_t GetPeerVehicleState(uint32_t p_peerId, int8_t p_charIndex) const;
	bool ValidateSessionLocations(uint16_t p_animIndex);

	void ResetAnimationState();
	void CancelLocalAnimInterest();
	void BroadcastChangedSessions(const std::vector<uint16_t>& p_changedAnims);
	void TickHostSessions();

	MessageHeader MakeHeader(uint8_t p_type, uint32_t p_target);
	void ProcessPendingRequests();
	void RemoveRemotePlayer(uint32_t p_peerId);
	void RemoveAllRemotePlayers();

	void CheckConnectionState();
	void AttemptReconnect();
	void ResetStateAfterReconnect();

	void NotifyPlayerCountChanged();
	void EnforceDisableNPCs();
	void PushAnimationState();

	// Serialize and send a fixed-size message via the transport
	template <typename T>
	void SendMessage(const T& p_msg);

	NetworkTransport* m_transport;
	PlatformCallbacks* m_callbacks;
	WorldStateSync m_worldSync;
	NameBubbleRenderer* m_localNameBubble;
	std::map<uint32_t, std::unique_ptr<RemotePlayer>> m_remotePlayers;
	std::map<LegoROI*, RemotePlayer*> m_roiToPlayer;

	uint32_t m_localPeerId;
	uint32_t m_hostPeerId;
	uint32_t m_sequence;
	uint32_t m_lastBroadcastTime;
	uint8_t m_lastValidActorId;
	bool m_localAllowRemoteCustomize;
	bool m_inIsleWorld;
	bool m_registered;

	std::atomic<bool> m_pendingToggleThirdPerson;
	std::atomic<bool> m_pendingToggleNameBubbles;
	std::atomic<int> m_pendingWalkAnim;
	std::atomic<int> m_pendingIdleAnim;
	std::atomic<int> m_pendingEmote;
	std::atomic<bool> m_pendingToggleAllowCustomize;
	std::atomic<int32_t> m_pendingAnimInterest;
	std::atomic<bool> m_pendingAnimCancel;

	bool m_showNameBubbles;
	bool m_lastCameraEnabled;
	uint8_t m_lastVehicleState;
	bool m_wasInRestrictedArea;

	// SI file reader (shared with animation loader)
	SIReader m_siReader;

	// NPC animation playback
	Multiplayer::Animation::Catalog m_animCatalog;
	Multiplayer::Animation::Loader m_animLoader;
	Multiplayer::Animation::LocationProximity m_locationProximity;
	Multiplayer::Animation::Coordinator m_animCoordinator;
	Multiplayer::Animation::SessionHost m_animSessionHost;
	int32_t m_localPendingAnimInterest;

	// Concurrent animation playback: one ScenePlayer per playing animation
	std::map<uint16_t, std::unique_ptr<Multiplayer::Animation::ScenePlayer>> m_playingAnims;

	// Pre-built completion JSON per playing animation (non-observer participants only).
	// Cached at animation start so it survives host migration/dropout.
	std::map<uint16_t, std::string> m_pendingCompletionJson;
	std::string BuildCompletionJson(uint16_t p_animIndex, uint64_t p_eventId);

	void TickAnimation();
	void StopScenePlayback(uint16_t p_animIndex, bool p_unlockRemotes);
	void StopAllPlayback();
	void NotifyAnimationsROIDestroyed(RemotePlayer* p_player);
	void UnlockRemotesForAnim(uint16_t p_animIndex);

	// Horn sound synchronization
	void PreloadHornSounds();
	void CleanupHornSounds();

	// Animation state push
	bool m_animStateDirty;
	bool m_animInterestDirty;
	uint32_t m_lastAnimPushTime;

	ConnectionState m_connectionState;
	bool m_wasRejected;
	std::string m_roomId;
	uint32_t m_reconnectAttempt;
	uint32_t m_reconnectDelay;
	uint32_t m_nextReconnectTime;

	static const uint32_t BROADCAST_INTERVAL_MS = 66; // ~15Hz
	static const uint32_t TIMEOUT_MS = 5000;          // 5 second timeout
	static const uint32_t RECONNECT_INITIAL_DELAY_MS = 1000;
	static const uint32_t RECONNECT_MAX_DELAY_MS = 30000;
	static const uint32_t RECONNECT_MAX_ATTEMPTS = 10;
	static const uint32_t ANIM_PUSH_COOLDOWN_MS = 250; // max ~4Hz for movement-based changes

	// Horn sound data
	static const int HORN_VEHICLE_COUNT = 4;
	class LegoCacheSound* m_hornTemplates[HORN_VEHICLE_COUNT];
	std::vector<class LegoCacheSound*> m_activeHorns;
};

} // namespace Multiplayer
