#pragma once

#include "extensions/multiplayer/animutils.h"
#include "extensions/multiplayer/protocol.h"
#include "mxtypes.h"

#include <cstdint>
#include <map>
#include <string>

class LegoROI;
class LegoWorld;
class LegoAnim;
class LegoTreeNode;

namespace Multiplayer
{

class NameBubbleRenderer;

class RemotePlayer {
public:
	RemotePlayer(uint32_t p_peerId, uint8_t p_actorId, uint8_t p_displayActorIndex);
	~RemotePlayer();

	void Spawn(LegoWorld* p_isleWorld);
	void Despawn();
	void UpdateFromNetwork(const PlayerStateMsg& p_msg);
	void Tick(float p_deltaTime);
	void ReAddToScene();

	uint32_t GetPeerId() const { return m_peerId; }
	uint8_t GetActorId() const { return m_actorId; }
	uint8_t GetDisplayActorIndex() const { return m_displayActorIndex; }
	void SetActorId(uint8_t p_actorId) { m_actorId = p_actorId; }
	bool IsSpawned() const { return m_spawned; }
	bool IsVisible() const { return m_visible; }
	int8_t GetWorldId() const { return m_targetWorldId; }
	uint32_t GetLastUpdateTime() const { return m_lastUpdateTime; }
	void SetVisible(bool p_visible);
	void TriggerEmote(uint8_t p_emoteId);
	void SetNameBubbleVisible(bool p_visible);
	void CreateNameBubble();
	void DestroyNameBubble();

private:
	using AnimCache = AnimUtils::AnimCache;

	AnimCache* GetOrBuildAnimCache(const char* p_animName);
	const char* GetDisplayActorName() const;
	void UpdateTransform(float p_deltaTime);
	void UpdateAnimation(float p_deltaTime);
	void UpdateVehicleState();
	void EnterVehicle(int8_t p_vehicleType);
	void ExitVehicle();

	uint32_t m_peerId;
	uint8_t m_actorId;
	uint8_t m_displayActorIndex;
	char m_uniqueName[32];
	char m_displayName[8];

	LegoROI* m_roi;
	bool m_spawned;
	bool m_visible;

	float m_targetPosition[3];
	float m_targetDirection[3];
	float m_targetUp[3];
	float m_targetSpeed;
	int8_t m_targetVehicleType;
	int8_t m_targetWorldId;
	uint32_t m_lastUpdateTime;
	bool m_hasReceivedUpdate;

	float m_currentPosition[3];
	float m_currentDirection[3];
	float m_currentUp[3];

	// Animation state
	uint8_t m_walkAnimId;
	uint8_t m_idleAnimId;
	AnimCache* m_walkAnimCache;
	AnimCache* m_idleAnimCache;
	float m_animTime;
	float m_idleTime;
	float m_idleAnimTime;
	bool m_wasMoving;

	// Emote state
	AnimCache* m_emoteAnimCache;
	float m_emoteTime;
	float m_emoteDuration;
	bool m_emoteActive;

	// ROI map cache: animation name -> cached ROI map (invalidated on world change)
	std::map<std::string, AnimCache> m_animCacheMap;

	// Ride animation (vehicle-specific, not cached globally)
	LegoAnim* m_rideAnim;
	LegoROI** m_rideRoiMap;
	MxU32 m_rideRoiMapSize;
	LegoROI* m_rideVehicleROI;

	LegoROI* m_vehicleROI;
	int8_t m_currentVehicleType;

	NameBubbleRenderer* m_nameBubble;
};

} // namespace Multiplayer
