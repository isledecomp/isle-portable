#include "extensions/multiplayer/remoteplayer.h"

#include "extensions/multiplayer/namebubblerenderer.h"

#include "3dmanager/lego3dmanager.h"
#include "anim/legoanim.h"
#include "extensions/multiplayer/charactercloner.h"
#include "legoactor.h"
#include "legoanimpresenter.h"
#include "legocharactermanager.h"
#include "legovideomanager.h"
#include "legoworld.h"
#include "misc.h"
#include "misc/legotree.h"
#include "mxgeometry/mxgeometry3d.h"
#include "realtime/realtime.h"
#include "roi/legoroi.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <cmath>
#include <vec.h>

using namespace Multiplayer;

RemotePlayer::RemotePlayer(uint32_t p_peerId, uint8_t p_actorId, uint8_t p_displayActorIndex)
	: m_peerId(p_peerId), m_actorId(p_actorId), m_displayActorIndex(p_displayActorIndex), m_roi(nullptr),
	  m_spawned(false), m_visible(false), m_targetSpeed(0.0f), m_targetVehicleType(VEHICLE_NONE), m_targetWorldId(-1),
	  m_lastUpdateTime(SDL_GetTicks()), m_hasReceivedUpdate(false), m_walkAnimId(0), m_idleAnimId(0),
	  m_walkAnimCache(nullptr), m_idleAnimCache(nullptr), m_animTime(0.0f), m_idleTime(0.0f), m_idleAnimTime(0.0f),
	  m_wasMoving(false), m_emoteAnimCache(nullptr), m_emoteTime(0.0f), m_emoteDuration(0.0f), m_emoteActive(false),
	  m_rideAnim(nullptr), m_rideRoiMap(nullptr), m_rideRoiMapSize(0), m_rideVehicleROI(nullptr), m_vehicleROI(nullptr),
	  m_currentVehicleType(VEHICLE_NONE), m_nameBubble(nullptr)
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

	m_roi = CharacterCloner::Clone(charMgr, m_uniqueName, actorName);
	if (!m_roi) {
		return;
	}

	VideoManager()->Get3DManager()->Add(*m_roi);
	VideoManager()->Get3DManager()->Moved(*m_roi);

	m_roi->SetVisibility(FALSE);
	m_spawned = true;
	m_visible = false;

	// Build initial walk and idle animation caches
	m_walkAnimCache = GetOrBuildAnimCache(g_walkAnimNames[m_walkAnimId]);
	m_idleAnimCache = GetOrBuildAnimCache(g_idleAnimNames[m_idleAnimId]);

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

	DestroyNameBubble();
	ExitVehicle();

	if (m_roi) {
		VideoManager()->Get3DManager()->Remove(*m_roi);
		CharacterManager()->ReleaseActor(m_uniqueName);
		m_roi = nullptr;
	}

	// Clear cached animation ROI maps (anim pointers are world-owned).
	m_animCacheMap.clear();
	m_walkAnimCache = nullptr;
	m_idleAnimCache = nullptr;
	m_emoteAnimCache = nullptr;
	m_emoteActive = false;

	m_spawned = false;
	m_visible = false;
}

const char* RemotePlayer::GetDisplayActorName() const
{
	if (IsValidDisplayActorIndex(m_displayActorIndex)) {
		return CharacterManager()->GetActorName(m_displayActorIndex);
	}
	return LegoActor::GetActorName(m_actorId);
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
		m_hasReceivedUpdate = true;
	}

	// Update display name (can change when player switches save file)
	char newName[8];
	SDL_memcpy(newName, p_msg.name, sizeof(newName));
	newName[sizeof(newName) - 1] = '\0';
	if (SDL_strcmp(m_displayName, newName) != 0) {
		SDL_Log(
			"RemotePlayer[%u] name changed: '%s' -> '%s' (spawned=%d)",
			m_peerId,
			m_displayName,
			newName,
			m_spawned
		);
		SDL_memcpy(m_displayName, newName, sizeof(m_displayName));

		// Recreate bubble with new name (or create for the first time)
		if (m_spawned) {
			DestroyNameBubble();
			CreateNameBubble();
		}
	}

	// Swap walk animation if changed
	if (p_msg.walkAnimId != m_walkAnimId && p_msg.walkAnimId < g_walkAnimCount) {
		m_walkAnimId = p_msg.walkAnimId;
		m_walkAnimCache = GetOrBuildAnimCache(g_walkAnimNames[m_walkAnimId]);
	}

	// Swap idle animation if changed
	if (p_msg.idleAnimId != m_idleAnimId && p_msg.idleAnimId < g_idleAnimCount) {
		m_idleAnimId = p_msg.idleAnimId;
		m_idleAnimCache = GetOrBuildAnimCache(g_idleAnimNames[m_idleAnimId]);
	}
}

void RemotePlayer::Tick(float p_deltaTime)
{
	if (!m_spawned || !m_visible) {
		return;
	}

	UpdateVehicleState();
	UpdateTransform(p_deltaTime);
	UpdateAnimation(p_deltaTime);

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
	if (m_rideVehicleROI) {
		VideoManager()->Get3DManager()->Add(*m_rideVehicleROI);
	}
}

void RemotePlayer::SetVisible(bool p_visible)
{
	if (!m_spawned || !m_roi) {
		return;
	}

	m_visible = p_visible;

	if (p_visible) {
		if (m_currentVehicleType != VEHICLE_NONE && IsLargeVehicle(m_currentVehicleType)) {
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
		if (m_rideVehicleROI) {
			m_rideVehicleROI->SetVisibility(FALSE);
		}
	}
}

RemotePlayer::AnimCache* RemotePlayer::GetOrBuildAnimCache(const char* p_animName)
{
	return AnimUtils::GetOrBuildAnimCache(m_animCacheMap, m_roi, p_animName);
}

void RemotePlayer::TriggerEmote(uint8_t p_emoteId)
{
	if (p_emoteId >= g_emoteAnimCount || !m_spawned) {
		return;
	}

	// Only play emotes when stationary
	if (m_targetSpeed > 0.01f) {
		return;
	}

	AnimCache* cache = GetOrBuildAnimCache(g_emoteAnimNames[p_emoteId]);
	if (!cache || !cache->anim) {
		return;
	}

	m_emoteAnimCache = cache;
	m_emoteTime = 0.0f;
	m_emoteDuration = (float) cache->anim->GetDuration();
	m_emoteActive = true;
}


void RemotePlayer::UpdateTransform(float p_deltaTime)
{
	LERP3(m_currentPosition, m_currentPosition, m_targetPosition, 0.2f);
	LERP3(m_currentDirection, m_currentDirection, m_targetDirection, 0.2f);
	LERP3(m_currentUp, m_currentUp, m_targetUp, 0.2f);

	// Negate the received direction to restore the backward-z ROI convention.
	// BroadcastLocalState sends visual-forward; negating here gives ROI z =
	// backward, so mesh faces -z = forward (matching the sender's visual).
	// See also: BroadcastLocalState in networkmanager.cpp.
	Mx3DPointFloat pos(m_currentPosition[0], m_currentPosition[1], m_currentPosition[2]);
	Mx3DPointFloat dir(-m_currentDirection[0], -m_currentDirection[1], -m_currentDirection[2]);
	Mx3DPointFloat up(m_currentUp[0], m_currentUp[1], m_currentUp[2]);

	MxMatrix mat;
	CalcLocalTransform(pos, dir, up, mat);

	m_roi->WrappedSetLocal2WorldWithWorldDataUpdate(mat);
	VideoManager()->Get3DManager()->Moved(*m_roi);

	if (m_vehicleROI && m_currentVehicleType != VEHICLE_NONE && IsLargeVehicle(m_currentVehicleType)) {
		m_vehicleROI->WrappedSetLocal2WorldWithWorldDataUpdate(mat);
		VideoManager()->Get3DManager()->Moved(*m_vehicleROI);
	}
}

void RemotePlayer::UpdateAnimation(float p_deltaTime)
{
	if (m_currentVehicleType != VEHICLE_NONE && IsLargeVehicle(m_currentVehicleType)) {
		return;
	}

	// Determine the active walk/ride animation and its ROI map
	LegoAnim* walkAnim = nullptr;
	LegoROI** walkRoiMap = nullptr;
	MxU32 walkRoiMapSize = 0;

	if (m_currentVehicleType != VEHICLE_NONE && m_rideAnim && m_rideRoiMap) {
		walkAnim = m_rideAnim;
		walkRoiMap = m_rideRoiMap;
		walkRoiMapSize = m_rideRoiMapSize;
	}
	else if (m_walkAnimCache && m_walkAnimCache->anim && m_walkAnimCache->roiMap) {
		walkAnim = m_walkAnimCache->anim;
		walkRoiMap = m_walkAnimCache->roiMap;
		walkRoiMapSize = m_walkAnimCache->roiMapSize;
	}

	// Ensure visibility of all mapped ROIs
	if (walkRoiMap) {
		AnimUtils::EnsureROIMapVisibility(walkRoiMap, walkRoiMapSize);
	}
	if (m_idleAnimCache && m_idleAnimCache->roiMap) {
		AnimUtils::EnsureROIMapVisibility(m_idleAnimCache->roiMap, m_idleAnimCache->roiMapSize);
	}

	bool inVehicle = (m_currentVehicleType != VEHICLE_NONE);
	bool isMoving = inVehicle || m_targetSpeed > 0.01f;

	// Movement interrupts emotes
	if (isMoving && m_emoteActive) {
		m_emoteActive = false;
		m_emoteAnimCache = nullptr;
	}

	if (isMoving) {
		// Walking / riding
		if (!walkAnim || !walkRoiMap) {
			return;
		}

		if (m_targetSpeed > 0.01f) {
			m_animTime += p_deltaTime * 2000.0f;
		}
		float duration = (float) walkAnim->GetDuration();
		if (duration > 0.0f) {
			float timeInCycle = m_animTime - duration * floorf(m_animTime / duration);

			MxMatrix transform(m_roi->GetLocal2World());
			LegoTreeNode* root = walkAnim->GetRoot();
			for (LegoU32 i = 0; i < root->GetNumChildren(); i++) {
				LegoROI::ApplyAnimationTransformation(root->GetChild(i), transform, (LegoTime) timeInCycle, walkRoiMap);
			}
		}
		m_wasMoving = true;
		m_idleTime = 0.0f;
		m_idleAnimTime = 0.0f;
	}
	else if (m_emoteActive && m_emoteAnimCache && m_emoteAnimCache->anim && m_emoteAnimCache->roiMap) {
		// Emote playback (one-shot)
		m_emoteTime += p_deltaTime * 1000.0f;

		if (m_emoteTime >= m_emoteDuration) {
			// Emote completed -- return to stationary flow
			m_emoteActive = false;
			m_emoteAnimCache = nullptr;
			m_wasMoving = false;
			m_idleTime = 0.0f;
			m_idleAnimTime = 0.0f;
		}
		else {
			MxMatrix transform(m_roi->GetLocal2World());
			LegoTreeNode* root = m_emoteAnimCache->anim->GetRoot();
			for (LegoU32 i = 0; i < root->GetNumChildren(); i++) {
				LegoROI::ApplyAnimationTransformation(
					root->GetChild(i),
					transform,
					(LegoTime) m_emoteTime,
					m_emoteAnimCache->roiMap
				);
			}
		}
	}
	else if (m_idleAnimCache && m_idleAnimCache->anim && m_idleAnimCache->roiMap) {
		// Idle animation
		if (m_wasMoving) {
			m_wasMoving = false;
			m_idleTime = 0.0f;
			m_idleAnimTime = 0.0f;
		}

		m_idleTime += p_deltaTime;

		// Hold standing pose for 2.5s, then loop breathing/swaying
		if (m_idleTime >= 2.5f) {
			m_idleAnimTime += p_deltaTime * 1000.0f;
		}

		float duration = (float) m_idleAnimCache->anim->GetDuration();
		if (duration > 0.0f) {
			float timeInCycle = m_idleAnimTime - duration * floorf(m_idleAnimTime / duration);

			MxMatrix transform(m_roi->GetLocal2World());
			LegoTreeNode* root = m_idleAnimCache->anim->GetRoot();
			for (LegoU32 i = 0; i < root->GetNumChildren(); i++) {
				LegoROI::ApplyAnimationTransformation(
					root->GetChild(i),
					transform,
					(LegoTime) timeInCycle,
					m_idleAnimCache->roiMap
				);
			}
		}
	}
}

void RemotePlayer::UpdateVehicleState()
{
	if (m_targetVehicleType != m_currentVehicleType) {
		if (m_currentVehicleType != VEHICLE_NONE) {
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

	m_currentVehicleType = p_vehicleType;
	m_animTime = 0.0f;

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
		const char* rideAnimName = g_rideAnimNames[p_vehicleType];
		const char* vehicleVariantName = g_rideVehicleROINames[p_vehicleType];

		if (!rideAnimName || !vehicleVariantName) {
			return;
		}

		LegoWorld* world = CurrentWorld();
		if (!world) {
			return;
		}

		MxCore* presenter = world->Find("LegoAnimPresenter", rideAnimName);
		if (!presenter) {
			return;
		}

		LegoAnimPresenter* animPresenter = static_cast<LegoAnimPresenter*>(presenter);
		m_rideAnim = animPresenter->GetAnimation();
		if (!m_rideAnim) {
			return;
		}

		// Use the base vehicle LOD (e.g. "moto", "bike") which is always loaded as
		// a world object.  The ride-specific variant LODs (e.g. "motoni", "bikebd")
		// are only available when the original animation pipeline starts locally.
		const char* baseName = g_vehicleROINames[p_vehicleType];
		char variantName[48];
		SDL_snprintf(variantName, sizeof(variantName), "%s_mp_%u", vehicleVariantName, m_peerId);
		m_rideVehicleROI = CharacterManager()->CreateAutoROI(variantName, baseName, FALSE);

		// Rename to variant name so FindChildROI can match animation tree nodes
		// (e.g. "MOTONI" in the anim tree matches ROI named "motoni").
		if (m_rideVehicleROI) {
			m_rideVehicleROI->SetName(vehicleVariantName);
		}

		AnimUtils::BuildROIMap(m_rideAnim, m_roi, m_rideVehicleROI, m_rideRoiMap, m_rideRoiMapSize);
	}
}

void RemotePlayer::ExitVehicle()
{
	if (m_currentVehicleType == VEHICLE_NONE) {
		return;
	}

	if (m_vehicleROI) {
		VideoManager()->Get3DManager()->Remove(*m_vehicleROI);
		CharacterManager()->ReleaseAutoROI(m_vehicleROI);
		m_vehicleROI = nullptr;
	}

	if (m_rideRoiMap) {
		delete[] m_rideRoiMap;
		m_rideRoiMap = nullptr;
		m_rideRoiMapSize = 0;
	}
	if (m_rideVehicleROI) {
		VideoManager()->Get3DManager()->Remove(*m_rideVehicleROI);
		CharacterManager()->ReleaseAutoROI(m_rideVehicleROI);
		m_rideVehicleROI = nullptr;
	}
	m_rideAnim = nullptr;

	if (m_visible) {
		m_roi->SetVisibility(TRUE);
	}

	m_currentVehicleType = VEHICLE_NONE;
	m_animTime = 0.0f;
	m_wasMoving = false;
}

void RemotePlayer::CreateNameBubble()
{
	if (m_nameBubble || m_displayName[0] == '\0') {
		return;
	}

	m_nameBubble = new NameBubbleRenderer();
	m_nameBubble->Create(m_displayName);
}

void RemotePlayer::DestroyNameBubble()
{
	if (m_nameBubble) {
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
