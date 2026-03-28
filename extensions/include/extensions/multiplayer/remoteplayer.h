#pragma once

#include "extensions/common/characteranimator.h"
#include "extensions/common/customizestate.h"
#include "extensions/multiplayer/animation/catalog.h"
#include "extensions/multiplayer/protocol.h"
#include "mxgeometry/mxmatrix.h"
#include "mxtypes.h"

#include <cstdint>
#include <string>
#include <vector>

class LegoROI;
class LegoWorld;

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
	const char* GetUniqueName() const { return m_uniqueName; }
	uint8_t GetActorId() const { return m_actorId; }
	uint8_t GetDisplayActorIndex() const { return m_displayActorIndex; }
	void SetActorId(uint8_t p_actorId) { m_actorId = p_actorId; }
	LegoROI* GetROI() const { return m_roi; }
	bool IsSpawned() const { return m_spawned; }
	bool IsVisible() const { return m_visible; }
	int8_t GetWorldId() const { return m_targetWorldId; }
	const std::vector<int16_t>& GetLocations() const { return m_locations; }
	void SetLocations(std::vector<int16_t> p_locations) { m_locations = std::move(p_locations); }
	bool IsAtLocation(int16_t p_location) const;
	uint32_t GetLastUpdateTime() const { return m_lastUpdateTime; }
	void SetVisible(bool p_visible);
	void TriggerEmote(uint8_t p_emoteId);
	void SetNameBubbleVisible(bool p_visible);
	void CreateNameBubble();
	void DestroyNameBubble();

	const Extensions::Common::CustomizeState& GetCustomizeState() const { return m_customizeState; }
	bool GetAllowRemoteCustomize() const { return m_allowRemoteCustomize; }
	void SetClickAnimObjectId(MxU32 p_clickAnimObjectId) { m_animator.SetClickAnimObjectId(p_clickAnimObjectId); }
	void StopClickAnimation();
	bool IsInVehicle() const { return m_animator.IsInVehicle(); }
	LegoROI* GetRideVehicleROI() const { return m_animator.GetRideVehicleROI(); }
	bool IsMoving() const { return m_animator.IsInVehicle() || m_targetSpeed > 0.01f; }
	bool IsInMultiPartEmote() const { return m_animator.IsInMultiPartEmote(); }

	const char* GetDisplayName() const { return m_displayName; }

	void LockForAnimation(uint16_t p_animIndex) { m_lockedForAnimIndex = p_animIndex; }
	void UnlockFromAnimation(uint16_t p_animIndex)
	{
		if (m_lockedForAnimIndex == p_animIndex) {
			m_lockedForAnimIndex = Animation::ANIM_INDEX_NONE;
		}
	}
	void ForceUnlockAnimation() { m_lockedForAnimIndex = Animation::ANIM_INDEX_NONE; }
	bool IsAnimationLocked() const { return m_lockedForAnimIndex != Animation::ANIM_INDEX_NONE; }

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
	char m_displayName[USERNAME_BUFFER_SIZE];

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
	std::vector<int16_t> m_locations;

	float m_currentPosition[3];
	float m_currentDirection[3];
	float m_currentUp[3];

	Extensions::Common::CharacterAnimator m_animator;

	LegoROI* m_vehicleROI;
	bool m_vehicleROICloned;
	std::vector<MxMatrix> m_vehicleChildOffsets; // child-to-parent local offsets for cloned hierarchical ROIs

	NameBubbleRenderer* m_nameBubble;

	Extensions::Common::CustomizeState m_customizeState;
	bool m_allowRemoteCustomize;
	uint16_t m_lockedForAnimIndex;
};

} // namespace Multiplayer
