#include "extensions/multiplayer/thirdpersoncamera.h"

#include "3dmanager/lego3dmanager.h"
#include "anim/legoanim.h"
#include "islepathactor.h"
#include "legoanimpresenter.h"
#include "legocameracontroller.h"
#include "legocharactermanager.h"
#include "legovideomanager.h"
#include "legoworld.h"
#include "misc.h"
#include "misc/legotree.h"
#include "mxgeometry/mxgeometry3d.h"
#include "mxgeometry/mxmatrix.h"
#include "realtime/realtime.h"
#include "roi/legoroi.h"

#include <cmath>

using namespace Multiplayer;

ThirdPersonCamera::ThirdPersonCamera()
	: m_enabled(false), m_active(false), m_playerROI(nullptr), m_walkAnimId(0), m_idleAnimId(0),
	  m_walkAnimCache(nullptr), m_idleAnimCache(nullptr), m_animTime(0.0f), m_idleTime(0.0f), m_idleAnimTime(0.0f),
	  m_wasMoving(false), m_emoteAnimCache(nullptr), m_emoteTime(0.0f), m_emoteDuration(0.0f), m_emoteActive(false),
	  m_currentVehicleType(VEHICLE_NONE), m_rideAnim(nullptr), m_rideRoiMap(nullptr), m_rideRoiMapSize(0),
	  m_rideVehicleROI(nullptr)
{
}

void ThirdPersonCamera::Enable()
{
	m_enabled = true;
}

void ThirdPersonCamera::Disable()
{
	m_enabled = false;
	m_active = false;
	m_playerROI = nullptr;
	ClearRideAnimation();
	m_animCacheMap.clear();
	ClearAnimCaches();
}

void ThirdPersonCamera::OnActorEnter(IslePathActor* p_actor)
{
	if (!m_enabled) {
		return;
	}

	LegoPathActor* userActor = UserActor();
	if (static_cast<LegoPathActor*>(p_actor) != userActor) {
		return;
	}

	LegoROI* newROI = userActor->GetROI();
	if (!newROI) {
		return;
	}

	// Detect if we're entering a vehicle
	int8_t vehicleType = DetectVehicleType(userActor);

	if (vehicleType != VEHICLE_NONE) {
		// Large vehicles and helicopter: stay first-person with dashboard.
		// Track the vehicle type so OnActorExit can trigger reinit on exit.
		if (IsLargeVehicle(vehicleType) || vehicleType == VEHICLE_HELICOPTER) {
			// Hide the walking character ROI that we made visible earlier.
			// Enter() doesn't call Exit() on the previous actor, so our
			// OnActorExit never fires for the walking character.
			if (m_playerROI) {
				m_playerROI->SetVisibility(FALSE);
				VideoManager()->Get3DManager()->Remove(*m_playerROI);
			}
			m_currentVehicleType = vehicleType;
			m_active = false;
			return;
		}

		// Small vehicle: need the character ROI for ride animations.
		if (!m_playerROI) {
			return;
		}

		m_currentVehicleType = vehicleType;
		m_active = true;

		SetupCamera(userActor);
		BuildRideAnimation(vehicleType);
		return;
	}

	// Non-vehicle (walking character) entry
	m_playerROI = newROI;
	m_currentVehicleType = VEHICLE_NONE;
	m_active = true;

	// Make the player model visible (Enter() hid it for first-person)
	m_playerROI->SetVisibility(TRUE);

	// SpawnPlayer() removes the ROI from the 3D manager before calling Enter().
	// Re-add it so the character is actually rendered in third-person mode.
	VideoManager()->Get3DManager()->Remove(*m_playerROI);
	VideoManager()->Get3DManager()->Add(*m_playerROI);

	// Build animation caches
	m_walkAnimCache = GetOrBuildAnimCache(g_walkAnimNames[m_walkAnimId]);
	m_idleAnimCache = GetOrBuildAnimCache(g_idleAnimNames[m_idleAnimId]);

	// Reset animation state
	m_animTime = 0.0f;
	m_idleTime = 0.0f;
	m_idleAnimTime = 0.0f;
	m_wasMoving = false;
	m_emoteActive = false;

	ApplyIdleFrame0();

	SetupCamera(userActor);
}

void ThirdPersonCamera::OnActorExit(IslePathActor* p_actor)
{
	if (!m_enabled) {
		return;
	}

	// The hook fires at the end of Exit(), after UserActor() has been restored
	// to the walking character. For vehicle exit, p_actor is the vehicle (not
	// UserActor), so we check m_currentVehicleType instead of comparing actors.
	if (m_currentVehicleType != VEHICLE_NONE) {
		// Exiting a vehicle: reinitialize immediately for the walking character.
		ClearRideAnimation();
		ClearAnimCaches();
		m_animCacheMap.clear();
		ReinitForCharacter();
	}
	else if (m_active && static_cast<LegoPathActor*>(p_actor) == UserActor()) {
		// Exiting on foot (e.g., world transition): full teardown.
		// Hide the player ROI and remove it from the 3D manager (we added it
		// in OnActorEnter so the character would render in third-person).
		if (m_playerROI) {
			m_playerROI->SetVisibility(FALSE);
			VideoManager()->Get3DManager()->Remove(*m_playerROI);
		}
		ClearRideAnimation();
		ClearAnimCaches();
		m_currentVehicleType = VEHICLE_NONE;
		m_playerROI = nullptr;
		m_active = false;
	}
}

void ThirdPersonCamera::Tick(float p_deltaTime)
{
	if (!m_active) {
		return;
	}

	if (!m_playerROI) {
		return;
	}

	// Small vehicle with ride animation (like RemotePlayer)
	if (m_currentVehicleType != VEHICLE_NONE) {
		if (m_rideAnim && m_rideRoiMap) {
			LegoPathActor* actor = UserActor();
			if (!actor || !actor->GetROI()) {
				return;
			}

			// Force visibility of ride ROI map entries
			AnimUtils::EnsureROIMapVisibility(m_rideRoiMap, m_rideRoiMapSize);

			// Only advance animation time when actually moving
			float speed = actor->GetWorldSpeed();
			if (fabsf(speed) > 0.01f) {
				m_animTime += p_deltaTime * 2000.0f;
			}

			// Use vehicle actor's transform as base (character ROI may be at old position)
			MxMatrix transform(actor->GetROI()->GetLocal2World());

			// Position character ROI at the vehicle so bones render at the right place
			m_playerROI->WrappedSetLocal2WorldWithWorldDataUpdate(transform);
			m_playerROI->SetVisibility(TRUE);

			float duration = (float) m_rideAnim->GetDuration();
			if (duration > 0.0f) {
				float timeInCycle = m_animTime - duration * floorf(m_animTime / duration);

				LegoTreeNode* root = m_rideAnim->GetRoot();
				for (LegoU32 i = 0; i < root->GetNumChildren(); i++) {
					LegoROI::ApplyAnimationTransformation(
						root->GetChild(i),
						transform,
						(LegoTime) timeInCycle,
						m_rideRoiMap
					);
				}
			}
		}
		return;
	}

	LegoPathActor* userActor = UserActor();
	if (!userActor) {
		return;
	}

	// Determine the active walk animation and its ROI map
	LegoAnim* walkAnim = nullptr;
	LegoROI** walkRoiMap = nullptr;
	MxU32 walkRoiMapSize = 0;

	if (m_walkAnimCache && m_walkAnimCache->anim && m_walkAnimCache->roiMap) {
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

	float speed = userActor->GetWorldSpeed();
	bool isMoving = fabsf(speed) > 0.01f;

	// Movement interrupts emotes
	if (isMoving && m_emoteActive) {
		m_emoteActive = false;
		m_emoteAnimCache = nullptr;
	}

	if (isMoving) {
		if (!walkAnim || !walkRoiMap) {
			return;
		}

		m_animTime += p_deltaTime * 2000.0f;
		float duration = (float) walkAnim->GetDuration();
		if (duration > 0.0f) {
			float timeInCycle = m_animTime - duration * floorf(m_animTime / duration);

			MxMatrix transform(m_playerROI->GetLocal2World());
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
		m_emoteTime += p_deltaTime * 1000.0f;

		if (m_emoteTime >= m_emoteDuration) {
			m_emoteActive = false;
			m_emoteAnimCache = nullptr;
			m_wasMoving = false;
			m_idleTime = 0.0f;
			m_idleAnimTime = 0.0f;
		}
		else {
			// Use the saved clean parent transform to prevent scale
			// accumulation (see TriggerEmote for details).
			MxMatrix transform(m_emoteParentTransform);

			LegoTreeNode* root = m_emoteAnimCache->anim->GetRoot();
			for (LegoU32 i = 0; i < root->GetNumChildren(); i++) {
				LegoROI::ApplyAnimationTransformation(
					root->GetChild(i),
					transform,
					(LegoTime) m_emoteTime,
					m_emoteAnimCache->roiMap
				);
			}

			// Restore the player ROI's transform — the animation's root
			// node (ACTOR_01) wrote a scaled value into it.
			m_playerROI->WrappedSetLocal2WorldWithWorldDataUpdate(m_emoteParentTransform);
		}
	}
	else if (m_idleAnimCache && m_idleAnimCache->anim && m_idleAnimCache->roiMap) {
		if (m_wasMoving) {
			m_wasMoving = false;
			m_idleTime = 0.0f;
			m_idleAnimTime = 0.0f;
		}

		m_idleTime += p_deltaTime;

		if (m_idleTime >= 2.5f) {
			m_idleAnimTime += p_deltaTime * 1000.0f;
		}

		float duration = (float) m_idleAnimCache->anim->GetDuration();
		if (duration > 0.0f) {
			float timeInCycle = m_idleAnimTime - duration * floorf(m_idleAnimTime / duration);

			MxMatrix transform(m_playerROI->GetLocal2World());
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

void ThirdPersonCamera::SetWalkAnimId(uint8_t p_id)
{
	if (p_id >= g_walkAnimCount) {
		return;
	}

	if (p_id != m_walkAnimId) {
		m_walkAnimId = p_id;
		if (m_active) {
			m_walkAnimCache = GetOrBuildAnimCache(g_walkAnimNames[m_walkAnimId]);
		}
	}
}

void ThirdPersonCamera::SetIdleAnimId(uint8_t p_id)
{
	if (p_id >= g_idleAnimCount) {
		return;
	}

	if (p_id != m_idleAnimId) {
		m_idleAnimId = p_id;
		if (m_active) {
			m_idleAnimCache = GetOrBuildAnimCache(g_idleAnimNames[m_idleAnimId]);
		}
	}
}

void ThirdPersonCamera::TriggerEmote(uint8_t p_emoteId)
{
	if (p_emoteId >= g_emoteAnimCount || !m_active) {
		return;
	}

	LegoPathActor* userActor = UserActor();
	if (!userActor || fabsf(userActor->GetWorldSpeed()) > 0.01f) {
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

	// Save the clean parent transform before the emote starts.
	// The emote animation's root node (ACTOR_01) maps to the player ROI,
	// so ApplyAnimationTransformation writes a scaled transform into
	// m_playerROI->m_local2world each frame.  When the character is
	// stationary the engine's CalculateTransform does not run, so the ROI
	// is never reset — causing the scale to compound across frames.
	// Using the saved clean transform as parent prevents this feedback.
	m_emoteParentTransform = m_playerROI->GetLocal2World();
}

void ThirdPersonCamera::OnWorldEnabled(LegoWorld* p_world)
{
	if (!m_enabled || !p_world) {
		return;
	}

	// Clear stale caches (animation presenters may have been recreated)
	m_animCacheMap.clear();
	ClearAnimCaches();

	ReinitForCharacter();
}

void ThirdPersonCamera::OnWorldDisabled(LegoWorld* p_world)
{
	if (!p_world) {
		return;
	}

	m_active = false;
	m_playerROI = nullptr;
	ClearRideAnimation();
	m_animCacheMap.clear();
	ClearAnimCaches();
}

ThirdPersonCamera::AnimCache* ThirdPersonCamera::GetOrBuildAnimCache(const char* p_animName)
{
	return AnimUtils::GetOrBuildAnimCache(m_animCacheMap, m_playerROI, p_animName);
}

void ThirdPersonCamera::ClearAnimCaches()
{
	m_walkAnimCache = nullptr;
	m_idleAnimCache = nullptr;
	m_emoteAnimCache = nullptr;
	m_emoteActive = false;
}

void ThirdPersonCamera::SetupCamera(LegoPathActor* p_actor)
{
	LegoWorld* world = CurrentWorld();
	if (!world || !world->GetCameraController()) {
		return;
	}

	// After Enter()'s TurnAround, the ROI direction is negated.
	// The mesh faces -z (local) = +path_forward (correct visual facing).
	// +z in ROI-local is the negated direction, i.e. behind the visual model.
	// Movement inversion is handled by ShouldInvertMovement in CalculateTransform.
	Mx3DPointFloat at(0.0f, 2.5f, 3.0f);
	Mx3DPointFloat dir(0.0f, -0.3f, -1.0f);
	Mx3DPointFloat up(0.0f, 1.0f, 0.0f);

	world->GetCameraController()->SetWorldTransform(at, dir, up);
	p_actor->TransformPointOfView();
}

void ThirdPersonCamera::BuildRideAnimation(int8_t p_vehicleType)
{
	if (p_vehicleType < 0 || p_vehicleType >= VEHICLE_COUNT) {
		return;
	}

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

	m_rideAnim = static_cast<LegoAnimPresenter*>(presenter)->GetAnimation();
	if (!m_rideAnim) {
		return;
	}

	// Create variant ROI from base vehicle name, rename for anim tree matching
	const char* baseName = g_vehicleROINames[p_vehicleType];
	m_rideVehicleROI = CharacterManager()->CreateAutoROI("tp_vehicle", baseName, FALSE);
	if (m_rideVehicleROI) {
		m_rideVehicleROI->SetName(vehicleVariantName);
	}

	AnimUtils::BuildROIMap(m_rideAnim, m_playerROI, m_rideVehicleROI, m_rideRoiMap, m_rideRoiMapSize);
	m_animTime = 0.0f;
}

void ThirdPersonCamera::ClearRideAnimation()
{
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
	m_currentVehicleType = VEHICLE_NONE;
}

void ThirdPersonCamera::ApplyIdleFrame0()
{
	if (!m_playerROI || !m_idleAnimCache || !m_idleAnimCache->anim || !m_idleAnimCache->roiMap) {
		return;
	}

	MxMatrix transform(m_playerROI->GetLocal2World());
	LegoTreeNode* root = m_idleAnimCache->anim->GetRoot();
	for (LegoU32 i = 0; i < root->GetNumChildren(); i++) {
		LegoROI::ApplyAnimationTransformation(root->GetChild(i), transform, (LegoTime) 0.0f, m_idleAnimCache->roiMap);
	}
}

void ThirdPersonCamera::ReinitForCharacter()
{
	LegoPathActor* userActor = UserActor();
	if (!userActor) {
		m_active = false;
		return;
	}

	LegoROI* roi = userActor->GetROI();
	if (!roi) {
		m_active = false;
		return;
	}

	int8_t vehicleType = DetectVehicleType(userActor);

	// Large vehicles and helicopter: stay first-person
	if (vehicleType == VEHICLE_HELICOPTER || (vehicleType != VEHICLE_NONE && IsLargeVehicle(vehicleType))) {
		m_active = false;
		return;
	}

	m_currentVehicleType = vehicleType;

	if (vehicleType != VEHICLE_NONE) {
		if (!m_playerROI) {
			m_active = false;
			return;
		}
		m_active = true;
		SetupCamera(userActor);
		BuildRideAnimation(vehicleType);
		return;
	}

	// Reinitializing for walking character
	m_playerROI = roi;
	m_playerROI->SetVisibility(TRUE);

	// Ensure the ROI is in the 3D manager so it gets rendered
	VideoManager()->Get3DManager()->Remove(*m_playerROI);
	VideoManager()->Get3DManager()->Add(*m_playerROI);

	m_walkAnimCache = GetOrBuildAnimCache(g_walkAnimNames[m_walkAnimId]);
	m_idleAnimCache = GetOrBuildAnimCache(g_idleAnimNames[m_idleAnimId]);

	m_animTime = 0.0f;
	m_idleTime = 0.0f;
	m_idleAnimTime = 0.0f;
	m_wasMoving = false;
	m_emoteActive = false;
	m_active = true;

	ApplyIdleFrame0();
	SetupCamera(userActor);
}
