#pragma once

#include "extensions/multiplayer/characteranimator.h"
#include "extensions/multiplayer/customizestate.h"
#include "extensions/multiplayer/protocol.h"
#include "mxtypes.h"

#include <cstdint>
#include <string>

class LegoROI;
class LegoWorld;

namespace Multiplayer
{

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
	const char* GetUniqueName() const { return m_uniqueName; }
	uint8_t GetActorId() const { return m_actorId; }
	uint8_t GetDisplayActorIndex() const { return m_displayActorIndex; }
	void SetActorId(uint8_t p_actorId) { m_actorId = p_actorId; }
	LegoROI* GetROI() const { return m_roi; }
	bool IsSpawned() const { return m_spawned; }
	bool IsVisible() const { return m_visible; }
	int8_t GetWorldId() const { return m_targetWorldId; }
	uint32_t GetLastUpdateTime() const { return m_lastUpdateTime; }
	void SetVisible(bool p_visible);
	void TriggerEmote(uint8_t p_emoteId);
	void SetNameBubbleVisible(bool p_visible);
	void CreateNameBubble();
	void DestroyNameBubble();

	const CustomizeState& GetCustomizeState() const { return m_customizeState; }
	bool GetAllowRemoteCustomize() const { return m_allowRemoteCustomize; }
	void SetClickAnimObjectId(MxU32 p_clickAnimObjectId) { m_animator.SetClickAnimObjectId(p_clickAnimObjectId); }
	void StopClickAnimation();
	bool IsInVehicle() const { return m_animator.IsInVehicle(); }
	bool IsMoving() const { return m_animator.IsInVehicle() || m_targetSpeed > 0.01f; }

private:
	const char* GetDisplayActorName() const;
	void UpdateTransform(float p_deltaTime);
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

	CharacterAnimator m_animator;

	LegoROI* m_vehicleROI;

	CustomizeState m_customizeState;
	bool m_allowRemoteCustomize;
};

} // namespace Multiplayer
