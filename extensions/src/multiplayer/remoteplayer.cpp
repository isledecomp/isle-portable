#include "extensions/multiplayer/remoteplayer.h"

#include "3dmanager/lego3dmanager.h"
#include "extensions/common/arearestriction.h"
#include "extensions/common/charactercloner.h"
#include "extensions/common/charactercustomizer.h"
#include "extensions/multiplayer/namebubblerenderer.h"
#include "legocharactermanager.h"
#include "legovideomanager.h"
#include "legoworld.h"
#include "misc.h"
#include "mxgeometry/mxgeometry3d.h"
#include "realtime/realtime.h"
#include "roi/legoroi.h"

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <algorithm>
#include <vec.h>

using namespace Extensions;
using namespace Multiplayer;
using Common::DetectVehicleType;
using Common::g_idleAnimCount;
using Common::g_vehicleROINames;
using Common::g_walkAnimCount;
using Common::IsLargeVehicle;
using Common::WORLD_NOT_VISIBLE;

RemotePlayer::RemotePlayer(uint32_t p_peerId, uint8_t p_actorId, uint8_t p_displayActorIndex)
	: m_peerId(p_peerId), m_actorId(p_actorId), m_displayActorIndex(p_displayActorIndex), m_roi(nullptr),
	  m_spawned(false), m_visible(false), m_targetSpeed(0.0f), m_targetVehicleType(VEHICLE_NONE),
	  m_targetWorldId(WORLD_NOT_VISIBLE), m_lastUpdateTime(SDL_GetTicks()), m_hasReceivedUpdate(false),
	  m_animator(Common::CharacterAnimatorConfig{/*.saveEmoteTransform=*/false, /*.propSuffix=*/p_peerId}),
	  m_vehicleROI(nullptr), m_nameBubble(nullptr), m_allowRemoteCustomize(true),
	  m_lockedForAnimIndex(Animation::ANIM_INDEX_NONE)
{
	m_displayName[0] = '\0';
	const char* displayName = GetDisplayActorName();
	SDL_snprintf(m_uniqueName, sizeof(m_uniqueName), "%s_mp_%u", displayName, p_peerId);

	ZEROVEC3(m_targetPosition);
	m_targetDirection[0] = 0.0f;
	m_targetDirection[1] = 0.0f;
	m_targetDirection[2] = 1.0f;
	m_targetUp[0] = 0.0f;
	m_targetUp[1] = 1.0f;
	m_targetUp[2] = 0.0f;

	SET3(m_currentPosition, m_targetPosition);
	SET3(m_currentDirection, m_targetDirection);
	SET3(m_currentUp, m_targetUp);
}

RemotePlayer::~RemotePlayer()
{
	Despawn();
}

bool RemotePlayer::IsAtLocation(int16_t p_location) const
{
	return std::find(m_locations.begin(), m_locations.end(), p_location) != m_locations.end();
}

void RemotePlayer::Spawn(LegoWorld* p_isleWorld)
{
	if (m_spawned) {
		return;
	}

	LegoCharacterManager* charMgr = CharacterManager();
	if (!charMgr) {
		return;
	}

	const char* actorName = GetDisplayActorName();
	if (!actorName) {
		return;
	}

	m_roi = Common::CharacterCloner::Clone(charMgr, m_uniqueName, actorName);
	if (!m_roi) {
		return;
	}

	VideoManager()->Get3DManager()->Add(*m_roi);
	VideoManager()->Get3DManager()->Moved(*m_roi);

	m_roi->SetVisibility(FALSE);
	m_spawned = true;
	m_visible = false;

	// Initialize customize state from the display actor's info
	uint8_t actorInfoIndex = Common::CharacterCustomizer::ResolveActorInfoIndex(m_displayActorIndex);
	m_customizeState.InitFromActorInfo(actorInfoIndex);

	// Build initial walk and idle animation caches
	m_animator.InitAnimCaches(m_roi);

	// Create name bubble if we already have a name
	if (m_displayName[0] != '\0') {
		CreateNameBubble();
	}
}

void RemotePlayer::Despawn()
{
	if (!m_spawned) {
		return;
	}

	m_animator.StopClickAnimation();
	DestroyNameBubble();
	ExitVehicle();

	if (m_roi) {
		VideoManager()->Get3DManager()->Remove(*m_roi);
		CharacterManager()->ReleaseActor(m_uniqueName);
		m_roi = nullptr;
	}

	// Clear cached animation ROI maps (anim pointers are world-owned).
	m_animator.ClearAll();

	m_spawned = false;
	m_visible = false;
}

const char* RemotePlayer::GetDisplayActorName() const
{
	return CharacterManager()->GetActorName(m_displayActorIndex);
}

void RemotePlayer::UpdateFromNetwork(const PlayerStateMsg& p_msg)
{
	float posDelta = SDL_sqrtf(DISTSQRD3(p_msg.position, m_targetPosition));

	SET3(m_targetPosition, p_msg.position);
	SET3(m_targetDirection, p_msg.direction);
	SET3(m_targetUp, p_msg.up);
	m_targetSpeed = posDelta > 0.01f ? posDelta : 0.0f;
	m_targetVehicleType = p_msg.vehicleType;
	m_targetWorldId = p_msg.worldId;
	m_lastUpdateTime = SDL_GetTicks();

	if (!m_hasReceivedUpdate) {
		SET3(m_currentPosition, m_targetPosition);
		SET3(m_currentDirection, m_targetDirection);
		SET3(m_currentUp, m_targetUp);
		m_targetSpeed = 0.0f; // No meaningful speed from first sample
		m_hasReceivedUpdate = true;
	}

	// Update display name (can change when player switches save file)
	char newName[8];
	SDL_memcpy(newName, p_msg.name, sizeof(newName));
	newName[sizeof(newName) - 1] = '\0';
	if (SDL_strcmp(m_displayName, newName) != 0) {
		SDL_memcpy(m_displayName, newName, sizeof(m_displayName));

		// Recreate bubble with new name (or create for the first time)
		if (m_spawned) {
			DestroyNameBubble();
			CreateNameBubble();
		}
	}

	// Update customize state from packed data
	Common::CustomizeState newState;
	newState.Unpack(p_msg.customizeData);

	if (newState != m_customizeState) {
		uint8_t actorInfoIndex = Common::CharacterCustomizer::ResolveActorInfoIndex(m_displayActorIndex);
		m_customizeState = newState;
		if (m_spawned && m_roi) {
			Common::CharacterCustomizer::ApplyFullState(m_roi, actorInfoIndex, m_customizeState);
		}
	}

	// Update allow remote customize flag
	m_allowRemoteCustomize = (p_msg.customizeFlags & 0x01) != 0;

	// Sync multi-part emote frozen state from remote
	bool isFrozen = (p_msg.customizeFlags & 0x02) != 0;
	int8_t frozenEmoteId = isFrozen ? (int8_t) ((p_msg.customizeFlags >> 2) & 0x07) : -1;
	if (frozenEmoteId != m_animator.GetFrozenEmoteId()) {
		m_animator.SetFrozenEmoteId(frozenEmoteId, m_roi);
	}

	// Swap walk animation if changed
	if (p_msg.walkAnimId != m_animator.GetWalkAnimId() && p_msg.walkAnimId < g_walkAnimCount) {
		m_animator.SetWalkAnimId(p_msg.walkAnimId, m_roi);
	}

	// Swap idle animation if changed
	if (p_msg.idleAnimId != m_animator.GetIdleAnimId() && p_msg.idleAnimId < g_idleAnimCount) {
		m_animator.SetIdleAnimId(p_msg.idleAnimId, m_roi);
	}
}

void RemotePlayer::Tick(float p_deltaTime)
{
	if (!m_spawned || !m_visible) {
		return;
	}

	// During animation playback, skip transform/animation updates (ScenePlayer drives
	// our ROI), but still update the name bubble so it follows the animated position.
	if (IsAnimationLocked()) {
		if (m_nameBubble) {
			m_nameBubble->Update(m_roi);
		}
		return;
	}

	UpdateVehicleState();
	UpdateTransform(p_deltaTime);

	bool isMoving = m_targetSpeed > 0.01f;
	if (m_animator.GetFrozenEmoteId() >= 0) {
		isMoving = false;
	}
	m_animator.Tick(p_deltaTime, m_roi, isMoving);

	// Update name bubble position and billboard orientation
	if (m_nameBubble) {
		m_nameBubble->Update(m_roi);
	}
}

void RemotePlayer::ReAddToScene()
{
	if (m_spawned && m_roi) {
		VideoManager()->Get3DManager()->Add(*m_roi);
	}
	if (m_vehicleROI) {
		VideoManager()->Get3DManager()->Add(*m_vehicleROI);
	}
	if (m_animator.GetRideVehicleROI()) {
		VideoManager()->Get3DManager()->Add(*m_animator.GetRideVehicleROI());
	}
}

void RemotePlayer::SetVisible(bool p_visible)
{
	if (!m_spawned || !m_roi) {
		return;
	}

	m_visible = p_visible;

	if (p_visible) {
		if (m_animator.GetCurrentVehicleType() != VEHICLE_NONE && IsLargeVehicle(m_animator.GetCurrentVehicleType())) {
			m_roi->SetVisibility(FALSE);
			if (m_vehicleROI) {
				m_vehicleROI->SetVisibility(TRUE);
			}
		}
		else {
			m_roi->SetVisibility(TRUE);
			if (m_vehicleROI) {
				m_vehicleROI->SetVisibility(FALSE);
			}
		}
	}
	else {
		m_roi->SetVisibility(FALSE);
		if (m_vehicleROI) {
			m_vehicleROI->SetVisibility(FALSE);
		}
		if (m_animator.GetRideVehicleROI()) {
			m_animator.GetRideVehicleROI()->SetVisibility(FALSE);
		}
	}
}

void RemotePlayer::TriggerEmote(uint8_t p_emoteId)
{
	if (!m_spawned) {
		return;
	}

	bool isMoving = m_targetSpeed > 0.01f;
	if (m_animator.GetFrozenEmoteId() >= 0) {
		isMoving = false;
	}
	m_animator.TriggerEmote(p_emoteId, m_roi, isMoving);
}

void RemotePlayer::UpdateTransform(float p_deltaTime)
{
	LERP3(m_currentPosition, m_currentPosition, m_targetPosition, 0.2f);
	LERP3(m_currentDirection, m_currentDirection, m_targetDirection, 0.2f);
	LERP3(m_currentUp, m_currentUp, m_targetUp, 0.2f);

	// The network sends forward-z (visual forward).  Character meshes face -z,
	// so negate to get backward-z for the ROI (mesh faces the correct way).
	Mx3DPointFloat pos(m_currentPosition[0], m_currentPosition[1], m_currentPosition[2]);
	Mx3DPointFloat dir(-m_currentDirection[0], -m_currentDirection[1], -m_currentDirection[2]);
	Mx3DPointFloat up(m_currentUp[0], m_currentUp[1], m_currentUp[2]);

	MxMatrix mat;
	CalcLocalTransform(pos, dir, up, mat);

	m_roi->WrappedSetLocal2WorldWithWorldDataUpdate(mat);
	VideoManager()->Get3DManager()->Moved(*m_roi);

	if (m_vehicleROI && m_animator.GetCurrentVehicleType() != VEHICLE_NONE &&
		IsLargeVehicle(m_animator.GetCurrentVehicleType())) {
		m_vehicleROI->WrappedSetLocal2WorldWithWorldDataUpdate(mat);
		VideoManager()->Get3DManager()->Moved(*m_vehicleROI);
	}
}

void RemotePlayer::UpdateVehicleState()
{
	if (m_targetVehicleType != m_animator.GetCurrentVehicleType()) {
		if (m_animator.GetCurrentVehicleType() != VEHICLE_NONE) {
			ExitVehicle();
		}
		if (m_targetVehicleType != VEHICLE_NONE) {
			EnterVehicle(m_targetVehicleType);
		}
	}
}

void RemotePlayer::EnterVehicle(int8_t p_vehicleType)
{
	if (p_vehicleType < 0 || p_vehicleType >= VEHICLE_COUNT) {
		return;
	}

	m_animator.SetCurrentVehicleType(p_vehicleType);
	m_animator.SetAnimTime(0.0f);

	if (IsLargeVehicle(p_vehicleType)) {
		char vehicleName[48];
		SDL_snprintf(vehicleName, sizeof(vehicleName), "%s_mp_%u", g_vehicleROINames[p_vehicleType], m_peerId);

		m_vehicleROI = CharacterManager()->CreateAutoROI(vehicleName, g_vehicleROINames[p_vehicleType], FALSE);
		if (m_vehicleROI) {
			m_roi->SetVisibility(FALSE);
			MxMatrix mat(m_roi->GetLocal2World());
			m_vehicleROI->WrappedSetLocal2WorldWithWorldDataUpdate(mat);
			m_vehicleROI->SetVisibility(m_visible ? TRUE : FALSE);
		}
	}
	else {
		m_animator.BuildRideAnimation(p_vehicleType, m_roi, m_peerId);
	}
}

void RemotePlayer::ExitVehicle()
{
	if (m_animator.GetCurrentVehicleType() == VEHICLE_NONE) {
		return;
	}

	if (m_vehicleROI) {
		VideoManager()->Get3DManager()->Remove(*m_vehicleROI);
		CharacterManager()->ReleaseAutoROI(m_vehicleROI);
		m_vehicleROI = nullptr;
	}

	m_animator.ClearRideAnimation();

	if (m_visible) {
		m_roi->SetVisibility(TRUE);
	}

	m_animator.SetAnimTime(0.0f);
}

void RemotePlayer::CreateNameBubble()
{
	if (!m_nameBubble) {
		m_nameBubble = new NameBubbleRenderer();
	}
	m_nameBubble->Create(m_displayName);
}

void RemotePlayer::DestroyNameBubble()
{
	if (m_nameBubble) {
		m_nameBubble->Destroy();
		delete m_nameBubble;
		m_nameBubble = nullptr;
	}
}

void RemotePlayer::SetNameBubbleVisible(bool p_visible)
{
	if (m_nameBubble) {
		m_nameBubble->SetVisible(p_visible);
	}
}

void RemotePlayer::StopClickAnimation()
{
	m_animator.StopClickAnimation();
}
