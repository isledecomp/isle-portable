#include "extensions/multiplayer/animation/sceneplayer.h"

#include "3dmanager/lego3dmanager.h"
#include "anim/legoanim.h"
#include "extensions/common/animutils.h"
#include "extensions/common/charactercloner.h"
#include "legoanimationmanager.h"
#include "legocameracontroller.h"
#include "legocharactermanager.h"
#include "legovideomanager.h"
#include "legoworld.h"
#include "misc.h"
#include "misc/legotree.h"
#include "mxbackgroundaudiomanager.h"
#include "mxgeometry/mxgeometry3d.h"
#include "realtime/realtime.h"
#include "roi/legoroi.h"

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <algorithm>
#include <cmath>
#include <deque>
#include <functional>
#include <vector>

using namespace Multiplayer::Animation;
namespace AnimUtils = Extensions::Common::AnimUtils;
using Extensions::Common::CharacterCloner;

// Defined in legoanimationmanager.cpp
extern LegoAnimationManager::Character g_characters[47];

enum VehicleCategory {
	e_bike,
	e_motorcycle,
	e_skateboard,
	e_unknownVehicle
};

static VehicleCategory GetVehicleCategory(MxU32 p_vehicleIdx)
{
	if (p_vehicleIdx <= 3) {
		return e_bike;
	}
	if (p_vehicleIdx <= 5) {
		return e_motorcycle;
	}
	if (p_vehicleIdx == 6) {
		return e_skateboard;
	}
	return e_unknownVehicle;
}

static bool MatchesCharacter(const std::string& p_actorName, int8_t p_charIndex)
{
	if (p_charIndex < 0 || p_charIndex >= (int8_t) sizeOfArray(g_characters)) {
		return false;
	}
	return !SDL_strcasecmp(p_actorName.c_str(), g_characters[p_charIndex].m_name);
}

ScenePlayer::ScenePlayer()
	: m_playing(false), m_rebaseComputed(false), m_startTime(0), m_currentData(nullptr), m_category(e_npcAnim),
	  m_animRootROI(nullptr), m_vehicleROI(nullptr), m_hiddenVehicleROI(nullptr), m_roiMap(nullptr), m_roiMapSize(0),
	  m_hasCamAnim(false), m_observerMode(false), m_hideOnStop(false)
{
}

ScenePlayer::~ScenePlayer()
{
	if (m_playing) {
		Stop();
	}
}

void ScenePlayer::SetupROIs(const AnimInfo* p_animInfo)
{
	LegoU32 numActors = m_currentData->anim->GetNumActors();
	std::vector<LegoROI*> createdROIs;
	std::vector<AnimUtils::ROIAlias> aliases;
	std::deque<std::string> aliasNames;

	std::vector<bool> participantMatched(m_participants.size(), false);

	auto addAlias = [&](const std::string& p_name, LegoROI* p_roi) {
		aliasNames.push_back(p_name);
		aliases.push_back({aliasNames.back().c_str(), p_roi});
		m_actorAliases.push_back({p_name, p_roi});
	};

	auto createProp = [&](const std::string& p_name, const char* p_lodName) -> LegoROI* {
		char uniqueName[64];
		SDL_snprintf(uniqueName, sizeof(uniqueName), "npc_prop_%s", p_name.c_str());
		LegoROI* roi = CharacterManager()->CreateAutoROI(uniqueName, p_lodName, FALSE);
		if (roi) {
			roi->SetName(p_name.c_str());
			createdROIs.push_back(roi);
		}
		return roi;
	};

	for (LegoU32 i = 0; i < numActors; i++) {
		const char* actorName = m_currentData->anim->GetActorName(i);
		LegoU32 actorType = m_currentData->anim->GetActorType(i);

		if (!actorName || *actorName == '\0') {
			continue;
		}

		const char* lookupName = (*actorName == '*') ? actorName + 1 : actorName;
		std::string lowered(lookupName);
		std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);

		if (actorType == LegoAnimActorEntry::e_managedLegoActor) {
			bool matched = false;

			for (size_t p = 0; p < m_participants.size(); p++) {
				if (participantMatched[p] || m_participants[p].IsSpectator()) {
					continue;
				}

				if (MatchesCharacter(lowered, m_participants[p].charIndex)) {
					participantMatched[p] = true;
					matched = true;
					addAlias(lowered, m_participants[p].roi);
					break;
				}
			}

			if (matched) {
				continue;
			}

			// No participant matched — create a clone
			char uniqueName[64];
			SDL_snprintf(uniqueName, sizeof(uniqueName), "npc_char_%s", lowered.c_str());
			LegoROI* roi = CharacterCloner::Clone(CharacterManager(), uniqueName, lowered.c_str());
			if (roi) {
				roi->SetName(lowered.c_str());
				VideoManager()->Get3DManager()->Add(*roi);
				createdROIs.push_back(roi);
			}
		}
		else if (actorType == LegoAnimActorEntry::e_managedInvisibleRoiTrimmed || actorType == LegoAnimActorEntry::e_sceneRoi1 || actorType == LegoAnimActorEntry::e_sceneRoi2) {
			createProp(lowered, AnimUtils::TrimLODSuffix(lowered).c_str());
		}
		else if (actorType == LegoAnimActorEntry::e_managedInvisibleRoi) {
			createProp(lowered, lowered.c_str());
		}
		else {
			// Type 0/1: check if this is a vehicle actor via ModelInfo flag
			LegoROI* roi = nullptr;
			bool isVehicleActor = false;

			for (uint8_t m = 0; m < p_animInfo->m_modelCount; m++) {
				if (p_animInfo->m_models[m].m_name &&
					!SDL_strcasecmp(lowered.c_str(), p_animInfo->m_models[m].m_name) &&
					p_animInfo->m_models[m].m_unk0x2c) {
					isVehicleActor = true;
					break;
				}
			}

			// Try matching a participant's vehicle by category
			if (isVehicleActor && !m_vehicleROI) {
				MxU32 animVehicleIdx;
				if (AnimationManager()->FindVehicle(lowered.c_str(), animVehicleIdx)) {
					for (size_t p = 0; p < m_participants.size(); p++) {
						if (!m_participants[p].vehicleROI) {
							continue;
						}

						MxU32 perfVehicleIdx;
						if (AnimationManager()->FindVehicle(m_participants[p].vehicleROI->GetName(), perfVehicleIdx)) {
							if (GetVehicleCategory(animVehicleIdx) == GetVehicleCategory(perfVehicleIdx)) {
								m_vehicleROI = m_participants[p].vehicleROI;
								addAlias(lowered, m_vehicleROI);
								roi = m_vehicleROI;
								break;
							}
						}
					}
				}
			}

			// Try creating as prop
			if (!roi) {
				roi = createProp(lowered, AnimUtils::TrimLODSuffix(lowered).c_str());
			}

			// Final fallback: borrow local player's vehicle via alias
			if (!roi && m_participants[0].vehicleROI && !m_vehicleROI) {
				m_vehicleROI = m_participants[0].vehicleROI;
				addAlias(lowered, m_vehicleROI);
			}
		}
	}

	m_propROIs = std::move(createdROIs);

	// Find root ROI: first non-spectator participant matched to an animation actor
	LegoROI* rootROI = nullptr;
	for (size_t p = 0; p < m_participants.size(); p++) {
		if (!m_participants[p].IsSpectator() && participantMatched[p]) {
			rootROI = m_participants[p].roi;
			break;
		}
	}

	if (!rootROI && !m_participants.empty()) {
		rootROI = m_participants[0].roi;
	}

	if (!rootROI) {
		return;
	}

	m_animRootROI = rootROI;

	// Collect extra ROIs (other matched participants + props + vehicle)
	std::vector<LegoROI*> extras;
	for (size_t p = 0; p < m_participants.size(); p++) {
		if (m_participants[p].roi != rootROI && participantMatched[p]) {
			extras.push_back(m_participants[p].roi);
		}
	}
	for (auto* propROI : m_propROIs) {
		extras.push_back(propROI);
	}
	if (m_vehicleROI) {
		extras.push_back(m_vehicleROI);
	}

	delete[] m_roiMap;
	m_roiMap = nullptr;
	m_roiMapSize = 0;

	AnimUtils::BuildROIMap(
		m_currentData->anim,
		rootROI,
		extras.empty() ? nullptr : extras.data(),
		(int) extras.size(),
		m_roiMap,
		m_roiMapSize,
		aliases.empty() ? nullptr : aliases.data(),
		(int) aliases.size()
	);
}

void ScenePlayer::Play(
	const AnimInfo* p_animInfo,
	AnimCategory p_category,
	const ParticipantROI* p_participants,
	uint8_t p_participantCount,
	bool p_observerMode
)
{
	if (m_playing) {
		Stop();
	}

	if (p_participantCount == 0 || !p_participants[0].roi || !p_animInfo) {
		return;
	}

	SceneAnimData* data = m_loader.EnsureCached(p_animInfo->m_objectId);
	if (!data || !data->anim) {
		return;
	}

	m_currentData = data;
	m_category = p_category;
	m_hideOnStop = data->hideOnStop;
	m_observerMode = p_observerMode;

	// Build participant list with saved transforms for restoration
	for (uint8_t i = 0; i < p_participantCount; i++) {
		ParticipantROI participant;
		participant.roi = p_participants[i].roi;
		participant.vehicleROI = p_participants[i].vehicleROI;
		participant.savedTransform = p_participants[i].roi->GetLocal2World();
		participant.savedName = p_participants[i].roi->GetName();
		participant.charIndex = p_participants[i].charIndex;
		m_participants.push_back(participant);
	}

	SetupROIs(p_animInfo);

	if (!m_roiMap) {
		m_currentData = nullptr;
		m_participants.clear();
		return;
	}

	ResolvePtAtCamROIs();
	m_phonemePlayer.Init(data->phonemeTracks, m_roiMap, m_roiMapSize, m_actorAliases);
	m_audioPlayer.Init(data->audioTracks);

	// Observers don't get camera control — they watch the animation from their own viewpoint
	m_hasCamAnim = (!m_observerMode && m_category == e_camAnim && m_currentData->anim->GetCamAnim() != nullptr);

	if (m_category == e_camAnim && !m_observerMode) {
		// Hide the player's ride vehicle — it would remain visible at the
		// pre-animation position while the player is teleported
		LegoROI* localVehicle = m_participants[0].vehicleROI;
		if (localVehicle && localVehicle != m_vehicleROI) {
			localVehicle->SetVisibility(FALSE);
			m_hiddenVehicleROI = localVehicle;
		}
	}

	m_startTime = 0;
	m_playing = true;

	BackgroundAudioManager()->LowerVolume();
}

void ScenePlayer::ComputeRebaseMatrix()
{
	if (!m_animRootROI) {
		m_rebaseMatrix.SetIdentity();
		m_rebaseComputed = true;
		return;
	}

	// Use the root performer's saved position as the rebase anchor
	MxMatrix targetTransform;
	targetTransform.SetIdentity();
	for (const auto& p : m_participants) {
		if (p.roi == m_animRootROI) {
			targetTransform = p.savedTransform;
			break;
		}
	}

	// Find the root ROI's world transform at time 0 by walking the animation tree
	std::function<bool(LegoTreeNode*, MxMatrix&)> findOrigin = [&](LegoTreeNode* node, MxMatrix& parentWorld) -> bool {
		LegoAnimNodeData* data = (LegoAnimNodeData*) node->GetData();
		MxU32 roiIdx = data ? data->GetROIIndex() : 0;

		MxMatrix localMat;
		LegoROI::CreateLocalTransform(data, 0, localMat);
		MxMatrix worldMat;
		worldMat.Product(localMat, parentWorld);

		if (roiIdx != 0 && m_roiMap[roiIdx] == m_animRootROI) {
			m_animPose0 = worldMat;
			return true;
		}
		for (LegoU32 i = 0; i < node->GetNumChildren(); i++) {
			if (findOrigin(node->GetChild(i), worldMat)) {
				return true;
			}
		}
		return false;
	};
	MxMatrix identity;
	identity.SetIdentity();
	findOrigin(m_currentData->anim->GetRoot(), identity);

	// Inverse of animPose0 (rigid body: transpose rotation, negate translated position)
	MxMatrix invAnimPose0;
	invAnimPose0.SetIdentity();
	for (int r = 0; r < 3; r++) {
		for (int c = 0; c < 3; c++) {
			invAnimPose0[r][c] = m_animPose0[c][r];
		}
	}
	for (int r = 0; r < 3; r++) {
		invAnimPose0[3][r] =
			-(invAnimPose0[0][r] * m_animPose0[3][0] + invAnimPose0[1][r] * m_animPose0[3][1] +
			  invAnimPose0[2][r] * m_animPose0[3][2]);
	}

	m_rebaseMatrix.Product(invAnimPose0, targetTransform);
	m_rebaseComputed = true;
}

void ScenePlayer::ResolvePtAtCamROIs()
{
	m_ptAtCamROIs.clear();
	if (!m_currentData || m_currentData->ptAtCamNames.empty() || !m_roiMap) {
		return;
	}

	for (const auto& name : m_currentData->ptAtCamNames) {
		for (MxU32 i = 1; i < m_roiMapSize; i++) {
			if (m_roiMap[i] && m_roiMap[i]->GetName() && !SDL_strcasecmp(name.c_str(), m_roiMap[i]->GetName())) {
				m_ptAtCamROIs.push_back(m_roiMap[i]);
				break;
			}
		}
	}
}

void ScenePlayer::ApplyPtAtCam()
{
	if (m_ptAtCamROIs.empty()) {
		return;
	}

	LegoWorld* world = CurrentWorld();
	if (!world || !world->GetCameraController()) {
		return;
	}

	// Same math as LegoAnimPresenter::PutFrame
	for (LegoROI* roi : m_ptAtCamROIs) {
		if (!roi) {
			continue;
		}

		MxMatrix mat(roi->GetLocal2World());

		Vector3 pos(mat[0]);
		Vector3 dir(mat[1]);
		Vector3 up(mat[2]);
		Vector3 und(mat[3]);

		float possqr = sqrt(pos.LenSquared());
		float dirsqr = sqrt(dir.LenSquared());
		float upsqr = sqrt(up.LenSquared());

		up = und;
		up -= world->GetCameraController()->GetWorldLocation();
		dir /= dirsqr;
		pos.EqualsCross(dir, up);
		pos.Unitize();
		up.EqualsCross(pos, dir);
		pos *= possqr;
		dir *= dirsqr;
		up *= upsqr;

		roi->SetLocal2World(mat);
		roi->WrappedUpdateWorldData();
	}
}

void ScenePlayer::Tick()
{
	if (!m_playing || !m_currentData || m_participants.empty()) {
		return;
	}

	if (m_startTime == 0) {
		m_startTime = SDL_GetTicks();
	}

	if (m_category == e_npcAnim && m_roiMap) {
		AnimUtils::EnsureROIMapVisibility(m_roiMap, m_roiMapSize);
	}

	float elapsed = (float) (SDL_GetTicks() - m_startTime);

	if (elapsed >= m_currentData->duration) {
		Stop();
		return;
	}

	// 1. Skeletal animation
	if (m_currentData->anim && m_roiMap) {
		if (!m_rebaseComputed) {
			if (m_category == e_camAnim) {
				// cam_anims use the action transform directly (keyframes are in world space)
				if (m_currentData->actionTransform.valid) {
					Mx3DPointFloat loc(
						m_currentData->actionTransform.location[0],
						m_currentData->actionTransform.location[1],
						m_currentData->actionTransform.location[2]
					);
					Mx3DPointFloat dir(
						m_currentData->actionTransform.direction[0],
						m_currentData->actionTransform.direction[1],
						m_currentData->actionTransform.direction[2]
					);
					Mx3DPointFloat up(
						m_currentData->actionTransform.up[0],
						m_currentData->actionTransform.up[1],
						m_currentData->actionTransform.up[2]
					);
					CalcLocalTransform(loc, dir, up, m_rebaseMatrix);
				}
				else {
					m_rebaseMatrix.SetIdentity();
				}
				m_rebaseComputed = true;
			}
			else {
				ComputeRebaseMatrix();
			}
		}

		AnimUtils::ApplyTree(m_currentData->anim, m_rebaseMatrix, (LegoTime) elapsed, m_roiMap);
	}

	// 2. Camera animation (cam_anim only)
	if (m_hasCamAnim) {
		MxMatrix camTransform(m_rebaseMatrix);
		m_currentData->anim->GetCamAnim()->CalculateCameraTransform((LegoFloat) elapsed, camTransform);

		LegoWorld* world = CurrentWorld();
		if (world && world->GetCameraController()) {
			world->GetCameraController()->TransformPointOfView(camTransform, FALSE);
		}
	}

	// 3. PTATCAM post-processing
	ApplyPtAtCam();

	// 4. Audio
	const char* audioROIName = m_animRootROI ? m_animRootROI->GetName() : nullptr;
	m_audioPlayer.Tick(elapsed, audioROIName);

	// 5. Phoneme frames
	m_phonemePlayer.Tick(elapsed, m_currentData->phonemeTracks);
}

void ScenePlayer::Stop()
{
	if (!m_playing) {
		return;
	}

	m_audioPlayer.Cleanup();
	m_phonemePlayer.Cleanup();

	if (m_hideOnStop && m_roiMap) {
		for (MxU32 i = 1; i < m_roiMapSize; i++) {
			if (m_roiMap[i]) {
				m_roiMap[i]->SetVisibility(FALSE);
			}
		}
	}

	if (m_hiddenVehicleROI) {
		m_hiddenVehicleROI->SetVisibility(TRUE);
		m_hiddenVehicleROI = nullptr;
	}

	CleanupProps();
	m_vehicleROI = nullptr;

	delete[] m_roiMap;
	m_roiMap = nullptr;
	m_roiMapSize = 0;

	for (auto& p : m_participants) {
		p.roi->WrappedSetLocal2WorldWithWorldDataUpdate(p.savedTransform);
		p.roi->SetVisibility(TRUE);
	}
	m_participants.clear();

	BackgroundAudioManager()->RaiseVolume();

	m_ptAtCamROIs.clear();
	m_actorAliases.clear();
	m_playing = false;
	m_rebaseComputed = false;
	m_currentData = nullptr;
	m_animRootROI = nullptr;
	m_hasCamAnim = false;
	m_observerMode = false;
	m_startTime = 0;
	m_hideOnStop = false;
}

void ScenePlayer::CleanupProps()
{
	for (auto* propROI : m_propROIs) {
		if (propROI) {
			CharacterManager()->ReleaseAutoROI(propROI);
		}
	}
	m_propROIs.clear();
}
