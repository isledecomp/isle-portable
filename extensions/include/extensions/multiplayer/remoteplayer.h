#pragma once

#include "extensions/multiplayer/protocol.h"
#include "mxtypes.h"

#include <cstdint>

class LegoROI;
class LegoWorld;
class LegoAnim;
class LegoTreeNode;

namespace Multiplayer
{

class RemotePlayer {
public:
	RemotePlayer(uint32_t p_peerId, uint8_t p_actorId);
	~RemotePlayer();

	void Spawn(LegoWorld* p_isleWorld);
	void Despawn();
	void UpdateFromNetwork(const PlayerStateMsg& p_msg);
	void Tick(float p_deltaTime);
	void ReAddToScene();

	uint32_t GetPeerId() const { return m_peerId; }
	uint8_t GetActorId() const { return m_actorId; }
	bool IsSpawned() const { return m_spawned; }
	bool IsVisible() const { return m_visible; }
	int8_t GetWorldId() const { return m_targetWorldId; }
	uint32_t GetLastUpdateTime() const { return m_lastUpdateTime; }

	void SetVisible(bool p_visible);

private:
	void BuildWalkROIMap(LegoWorld* p_isleWorld);
	void BuildROIMap(
		LegoAnim* p_anim,
		LegoROI* p_rootROI,
		LegoROI* p_extraROI,
		LegoROI**& p_roiMap,
		MxU32& p_roiMapSize
	);
	void UpdateTransform(float p_deltaTime);
	void UpdateAnimation(float p_deltaTime);
	void UpdateVehicleState();
	void EnterVehicle(int8_t p_vehicleType);
	void ExitVehicle();

	uint32_t m_peerId;
	uint8_t m_actorId;
	char m_uniqueName[32];

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

	LegoAnim* m_walkAnim;
	LegoROI** m_walkRoiMap;
	MxU32 m_walkRoiMapSize;
	float m_animTime;
	float m_idleTime;
	bool m_wasMoving;

	LegoAnim* m_idleAnim;
	LegoROI** m_idleRoiMap;
	MxU32 m_idleRoiMapSize;
	float m_idleAnimTime;

	LegoAnim* m_rideAnim;
	LegoROI** m_rideRoiMap;
	MxU32 m_rideRoiMapSize;
	LegoROI* m_rideVehicleROI;

	LegoROI* m_vehicleROI;
	int8_t m_currentVehicleType;
};

} // namespace Multiplayer
