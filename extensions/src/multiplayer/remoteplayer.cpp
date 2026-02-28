#include "extensions/multiplayer/remoteplayer.h"

#include "3dmanager/lego3dmanager.h"
#include "anim/legoanim.h"
#include "legoactor.h"
#include "legoanimpresenter.h"
#include "legocharactermanager.h"
#include "legovideomanager.h"
#include "legoworld.h"
#include "misc.h"
#include "misc/legotree.h"
#include "roi/legoroi.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <cmath>
#include <vector>

using namespace Multiplayer;

// Vehicle ROI LOD names, indexed by VehicleType enum
// Large vehicles: character hidden, show vehicle ROI only
// Small vehicles: character visible with ride animation
static const char* g_vehicleROINames[VEHICLE_COUNT] = {
	"copter",   // VEHICLE_HELICOPTER (large)
	"jsuser",   // VEHICLE_JETSKI     (large)
	"dunebugy", // VEHICLE_DUNEBUGGY  (large)
	"bike",     // VEHICLE_BIKE       (small)
	"board",    // VEHICLE_SKATEBOARD (small)
	"moto",     // VEHICLE_MOTOCYCLE  (small)
	"towtk",    // VEHICLE_TOWTRACK   (large)
	"ambul"     // VEHICLE_AMBULANCE  (large)
};

// Ride animation presenter names for small vehicles (NULL for large)
static const char* g_rideAnimNames[VEHICLE_COUNT] = {
	NULL,       // VEHICLE_HELICOPTER
	NULL,       // VEHICLE_JETSKI
	NULL,       // VEHICLE_DUNEBUGGY
	"CNs001Bd", // VEHICLE_BIKE
	"CNs001sk", // VEHICLE_SKATEBOARD
	"CNs011Ni", // VEHICLE_MOTOCYCLE
	NULL,       // VEHICLE_TOWTRACK
	NULL        // VEHICLE_AMBULANCE
};

// Vehicle variant ROI names used in ride animations (NULL for large)
static const char* g_rideVehicleROINames[VEHICLE_COUNT] = {
	NULL,     // VEHICLE_HELICOPTER
	NULL,     // VEHICLE_JETSKI
	NULL,     // VEHICLE_DUNEBUGGY
	"bikebd", // VEHICLE_BIKE
	"board",  // VEHICLE_SKATEBOARD
	"motoni", // VEHICLE_MOTOCYCLE
	NULL,     // VEHICLE_TOWTRACK
	NULL      // VEHICLE_AMBULANCE
};

static bool IsLargeVehicle(int8_t p_vehicleType)
{
	return p_vehicleType != VEHICLE_NONE && p_vehicleType < VEHICLE_COUNT && g_rideAnimNames[p_vehicleType] == NULL;
}

RemotePlayer::RemotePlayer(uint32_t p_peerId, uint8_t p_actorId)
	: m_peerId(p_peerId), m_actorId(p_actorId), m_roi(nullptr), m_spawned(false), m_visible(false), m_targetSpeed(0.0f),
	  m_targetVehicleType(VEHICLE_NONE), m_targetWorldId(-1), m_lastUpdateTime(SDL_GetTicks()),
	  m_hasReceivedUpdate(false), m_walkAnim(nullptr), m_walkRoiMap(nullptr), m_walkRoiMapSize(0),
	  m_animTime(0.0f), m_idleTime(0.0f), m_wasMoving(false), m_idleAnim(nullptr), m_idleRoiMap(nullptr),
	  m_idleRoiMapSize(0), m_idleAnimTime(0.0f), m_rideAnim(nullptr), m_rideRoiMap(nullptr),
	  m_rideRoiMapSize(0), m_rideVehicleROI(nullptr),
	  m_vehicleROI(nullptr), m_currentVehicleType(VEHICLE_NONE)
{
	SDL_snprintf(m_uniqueName, sizeof(m_uniqueName), "%s_mp_%u", LegoActor::GetActorName(p_actorId), p_peerId);

	SDL_memset(m_targetPosition, 0, sizeof(m_targetPosition));
	m_targetDirection[0] = 0.0f;
	m_targetDirection[1] = 0.0f;
	m_targetDirection[2] = 1.0f;
	m_targetUp[0] = 0.0f;
	m_targetUp[1] = 1.0f;
	m_targetUp[2] = 0.0f;

	SDL_memcpy(m_currentPosition, m_targetPosition, sizeof(m_targetPosition));
	SDL_memcpy(m_currentDirection, m_targetDirection, sizeof(m_targetDirection));
	SDL_memcpy(m_currentUp, m_targetUp, sizeof(m_targetUp));
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

	const char* actorName = LegoActor::GetActorName(m_actorId);
	if (!actorName) {
		SDL_Log("Multiplayer: failed to get actor name for id %d", m_actorId);
		return;
	}

	// Create a full multi-part character clone with body parts
	m_roi = charMgr->CreateCharacterClone(m_uniqueName, actorName);

	if (!m_roi) {
		SDL_Log("Multiplayer: failed to create character clone for %s", m_uniqueName);
		return;
	}

	// Add ROI to the 3D scene and notify the 3D manager
	VideoManager()->Get3DManager()->Add(*m_roi);
	VideoManager()->Get3DManager()->Moved(*m_roi);

	// Start hidden until we get a STATE update confirming worldId
	m_roi->SetVisibility(FALSE);
	m_spawned = true;
	m_visible = false;

	// Build walk animation ROI map
	BuildWalkROIMap(p_isleWorld);

	// Build idle animation ROI map (CNs008xx - breathing/swaying)
	MxCore* idlePresenter = p_isleWorld->Find("LegoAnimPresenter", "CNs008xx");
	if (idlePresenter) {
		m_idleAnim = static_cast<LegoAnimPresenter*>(idlePresenter)->GetAnimation();
		if (m_idleAnim) {
			BuildROIMap(m_idleAnim, m_roi, nullptr, m_idleRoiMap, m_idleRoiMapSize);
		}
	}

	SDL_Log(
		"Multiplayer: spawned remote player %s (roi=%p, walkRoiMap=%u, idleRoiMap=%u)",
		m_uniqueName,
		(void*) m_roi,
		m_walkRoiMapSize,
		m_idleRoiMapSize
	);
}

void RemotePlayer::Despawn()
{
	if (!m_spawned) {
		return;
	}

	// Clean up vehicle state first
	ExitVehicle();

	if (m_roi) {
		VideoManager()->Get3DManager()->Remove(*m_roi);
		CharacterManager()->ReleaseActor(m_uniqueName);
		m_roi = nullptr;
	}

	if (m_walkRoiMap) {
		delete[] m_walkRoiMap;
		m_walkRoiMap = nullptr;
		m_walkRoiMapSize = 0;
	}
	if (m_idleRoiMap) {
		delete[] m_idleRoiMap;
		m_idleRoiMap = nullptr;
		m_idleRoiMapSize = 0;
	}

	m_walkAnim = nullptr;
	m_idleAnim = nullptr;
	m_spawned = false;
	m_visible = false;

	SDL_Log("Multiplayer: despawned remote player %s", m_uniqueName);
}

void RemotePlayer::UpdateFromNetwork(const PlayerStateMsg& p_msg)
{
	// Compute speed from position delta (GetWorldSpeed clamps backward movement to 0)
	float dx = p_msg.position[0] - m_targetPosition[0];
	float dy = p_msg.position[1] - m_targetPosition[1];
	float dz = p_msg.position[2] - m_targetPosition[2];
	float posDelta = SDL_sqrtf(dx * dx + dy * dy + dz * dz);

	SDL_memcpy(m_targetPosition, p_msg.position, sizeof(float) * 3);
	SDL_memcpy(m_targetDirection, p_msg.direction, sizeof(float) * 3);
	SDL_memcpy(m_targetUp, p_msg.up, sizeof(float) * 3);
	m_targetSpeed = posDelta > 0.01f ? posDelta : 0.0f;
	m_targetVehicleType = p_msg.vehicleType;
	m_targetWorldId = p_msg.worldId;
	m_lastUpdateTime = SDL_GetTicks();

	if (!m_hasReceivedUpdate) {
		// Snap to position on first update (don't interpolate from origin)
		SDL_memcpy(m_currentPosition, m_targetPosition, sizeof(float) * 3);
		SDL_memcpy(m_currentDirection, m_targetDirection, sizeof(float) * 3);
		SDL_memcpy(m_currentUp, m_targetUp, sizeof(float) * 3);
		m_hasReceivedUpdate = true;
	}
}

void RemotePlayer::Tick(float p_deltaTime)
{
	if (!m_spawned || !m_visible) {
		return;
	}

	// Log first tick to confirm the player is being updated
	static uint32_t lastLoggedPeer = 0;
	if (lastLoggedPeer != m_peerId) {
		SDL_Log(
			"Multiplayer: first tick for %s pos=(%.1f,%.1f,%.1f) hasUpdate=%d",
			m_uniqueName,
			m_currentPosition[0],
			m_currentPosition[1],
			m_currentPosition[2],
			m_hasReceivedUpdate
		);
		lastLoggedPeer = m_peerId;
	}

	UpdateVehicleState();
	UpdateTransform(p_deltaTime);
	UpdateAnimation(p_deltaTime);
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
		// Visibility depends on vehicle state
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
		// Ride vehicle ROI visibility is managed by the animation (ApplyAnimationTransformation)
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

void RemotePlayer::BuildWalkROIMap(LegoWorld* p_isleWorld)
{
	if (!p_isleWorld) {
		return;
	}

	// Find the generic slow walk animation presenter "CNs001xx"
	MxCore* presenter = p_isleWorld->Find("LegoAnimPresenter", "CNs001xx");
	if (!presenter) {
		SDL_Log("Multiplayer: walk animation presenter CNs001xx not found");
		return;
	}

	LegoAnimPresenter* animPresenter = static_cast<LegoAnimPresenter*>(presenter);
	m_walkAnim = animPresenter->GetAnimation();

	if (!m_walkAnim) {
		SDL_Log("Multiplayer: walk animation data is null");
		return;
	}

	BuildROIMap(m_walkAnim, m_roi, nullptr, m_walkRoiMap, m_walkRoiMapSize);
}

// Traverse the animation tree, assign ROI indices, and collect matched ROIs.
// This mirrors the game's UpdateStructMapAndROIIndex approach: ROI indices
// are assigned at runtime via SetROIIndex() and are NOT pre-stored in animation
// data (m_roiIndex starts at 0 for all nodes).
static void AssignROIIndices(
	LegoTreeNode* p_node,
	LegoROI* p_parentROI,
	LegoROI* p_rootROI,
	LegoROI* p_extraROI,
	MxU32& p_nextIndex,
	std::vector<LegoROI*>& p_entries,
	int p_depth = 0
)
{
	LegoROI* roi = p_parentROI;
	LegoAnimNodeData* data = (LegoAnimNodeData*) p_node->GetData();
	const char* name = data ? data->GetName() : nullptr;

	SDL_Log(
		"Multiplayer: [ROIMap] depth=%d name='%s' parentROI=%p rootROI=%p children=%d",
		p_depth,
		name ? name : "(null)",
		(void*) p_parentROI,
		(void*) p_rootROI,
		p_node->GetNumChildren()
	);

	if (name != nullptr && *name != '-') {
		LegoROI* matchedROI = nullptr;

		if (*name == '*' || p_parentROI == nullptr) {
			// Root-level node: either "*pepper" style or "actor_01" style variable reference.
			// Game resolves via GetVariableOrIdentity + FindROI; we map directly to our clone.
			roi = p_rootROI;
			matchedROI = p_rootROI;
			SDL_Log("Multiplayer: [ROIMap] matched root node '%s' to rootROI", name);
		}
		else {
			// Body part → search in parent's ROI hierarchy
			matchedROI = p_parentROI->FindChildROI(name, p_parentROI);
			SDL_Log(
				"Multiplayer: [ROIMap] FindChildROI('%s', parentROI=%p) = %p",
				name,
				(void*) p_parentROI,
				(void*) matchedROI
			);
			if (matchedROI == nullptr && p_extraROI != nullptr) {
				// Try extra ROI hierarchy (vehicle variant for ride animations)
				matchedROI = p_extraROI->FindChildROI(name, p_extraROI);
			}
		}

		if (matchedROI != nullptr) {
			data->SetROIIndex(p_nextIndex);
			p_entries.push_back(matchedROI);
			p_nextIndex++;
		}
		else {
			data->SetROIIndex(0);
		}
	}

	for (MxS32 i = 0; i < p_node->GetNumChildren(); i++) {
		AssignROIIndices(p_node->GetChild(i), roi, p_rootROI, p_extraROI, p_nextIndex, p_entries, p_depth + 1);
	}
}

void RemotePlayer::BuildROIMap(
	LegoAnim* p_anim,
	LegoROI* p_rootROI,
	LegoROI* p_extraROI,
	LegoROI**& p_roiMap,
	MxU32& p_roiMapSize
)
{
	if (!p_anim || !p_rootROI) {
		return;
	}

	LegoTreeNode* root = p_anim->GetRoot();
	if (!root) {
		return;
	}

	// Traverse tree, assigning ROI indices and collecting matched ROIs
	MxU32 nextIndex = 1;
	std::vector<LegoROI*> entries;
	AssignROIIndices(root, nullptr, p_rootROI, p_extraROI, nextIndex, entries);

	if (entries.empty()) {
		return;
	}

	// Build the ROI map array (1-indexed; index 0 reserved as NULL)
	p_roiMapSize = entries.size() + 1;
	p_roiMap = new LegoROI*[p_roiMapSize];
	p_roiMap[0] = nullptr;
	for (MxU32 i = 0; i < entries.size(); i++) {
		p_roiMap[i + 1] = entries[i];
	}
}

void RemotePlayer::UpdateTransform(float p_deltaTime)
{
	// Interpolate position toward target
	float lerpFactor = 0.2f;

	for (int i = 0; i < 3; i++) {
		m_currentPosition[i] += (m_targetPosition[i] - m_currentPosition[i]) * lerpFactor;
		m_currentDirection[i] += (m_targetDirection[i] - m_currentDirection[i]) * lerpFactor;
		m_currentUp[i] += (m_targetUp[i] - m_currentUp[i]) * lerpFactor;
	}

	// Build transform using CalcLocalTransform convention from realtime.cpp:
	// z = normalize(dir), y = normalize(up), x = y×z, y = z×x
	// Non-player character clones need negated direction (see legopathactor.cpp:152)
	float z[3], y[3], x[3];
	z[0] = -m_currentDirection[0];
	z[1] = -m_currentDirection[1];
	z[2] = -m_currentDirection[2];

	float zLen = SDL_sqrtf(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
	if (zLen > 0.001f) {
		z[0] /= zLen;
		z[1] /= zLen;
		z[2] /= zLen;
	}

	float yLen = SDL_sqrtf(
		m_currentUp[0] * m_currentUp[0] + m_currentUp[1] * m_currentUp[1] + m_currentUp[2] * m_currentUp[2]
	);
	y[0] = yLen > 0.001f ? m_currentUp[0] / yLen : 0.0f;
	y[1] = yLen > 0.001f ? m_currentUp[1] / yLen : 1.0f;
	y[2] = yLen > 0.001f ? m_currentUp[2] / yLen : 0.0f;

	// x = y × z
	x[0] = y[1] * z[2] - y[2] * z[1];
	x[1] = y[2] * z[0] - y[0] * z[2];
	x[2] = y[0] * z[1] - y[1] * z[0];
	float xLen = SDL_sqrtf(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
	if (xLen > 0.001f) {
		x[0] /= xLen;
		x[1] /= xLen;
		x[2] /= xLen;
	}

	// y = z × x (re-orthogonalize)
	y[0] = z[1] * x[2] - z[2] * x[1];
	y[1] = z[2] * x[0] - z[0] * x[2];
	y[2] = z[0] * x[1] - z[1] * x[0];
	yLen = SDL_sqrtf(y[0] * y[0] + y[1] * y[1] + y[2] * y[2]);
	if (yLen > 0.001f) {
		y[0] /= yLen;
		y[1] /= yLen;
		y[2] /= yLen;
	}

	// Build 4x4 transform matrix [right, up, direction, position] as rows
	MxMatrix mat;
	mat[0][0] = x[0];
	mat[0][1] = x[1];
	mat[0][2] = x[2];
	mat[0][3] = 0.0f;
	mat[1][0] = y[0];
	mat[1][1] = y[1];
	mat[1][2] = y[2];
	mat[1][3] = 0.0f;
	mat[2][0] = z[0];
	mat[2][1] = z[1];
	mat[2][2] = z[2];
	mat[2][3] = 0.0f;
	mat[3][0] = m_currentPosition[0];
	mat[3][1] = m_currentPosition[1];
	mat[3][2] = m_currentPosition[2];
	mat[3][3] = 1.0f;

	m_roi->WrappedSetLocal2WorldWithWorldDataUpdate(mat);
	VideoManager()->Get3DManager()->Moved(*m_roi);

	// Also update vehicle ROI transform if in large vehicle
	if (m_vehicleROI && m_currentVehicleType != VEHICLE_NONE && IsLargeVehicle(m_currentVehicleType)) {
		m_vehicleROI->WrappedSetLocal2WorldWithWorldDataUpdate(mat);
		VideoManager()->Get3DManager()->Moved(*m_vehicleROI);
	}
}

void RemotePlayer::UpdateAnimation(float p_deltaTime)
{
	// Determine which animation and ROI map to use
	LegoAnim* anim = nullptr;

	if (m_currentVehicleType != VEHICLE_NONE && IsLargeVehicle(m_currentVehicleType)) {
		// Large vehicle: no animation, character is hidden
		return;
	}

	LegoROI** roiMap = nullptr;

	if (m_currentVehicleType != VEHICLE_NONE && m_rideAnim && m_rideRoiMap) {
		// Small vehicle: use ride animation
		anim = m_rideAnim;
		roiMap = m_rideRoiMap;
	}
	else if (m_walkAnim && m_walkRoiMap) {
		// On foot: use walk animation
		anim = m_walkAnim;
		roiMap = m_walkRoiMap;
	}
	else {
		return;
	}

	// Ensure all body parts are visible before animation (matches game's AnimateWithTransform)
	MxU32 roiMapSize = (roiMap == m_walkRoiMap) ? m_walkRoiMapSize : m_rideRoiMapSize;
	MxU32 idleMapSize = m_idleRoiMapSize;
	for (MxU32 i = 1; i < roiMapSize; i++) {
		if (roiMap[i] != nullptr) {
			roiMap[i]->SetVisibility(TRUE);
		}
	}
	// Also ensure idle ROI map parts are visible (may include different body parts)
	for (MxU32 i = 1; i < idleMapSize; i++) {
		if (m_idleRoiMap[i] != nullptr) {
			m_idleRoiMap[i]->SetVisibility(TRUE);
		}
	}

	bool inVehicle = (m_currentVehicleType != VEHICLE_NONE);

	if (inVehicle || m_targetSpeed > 0.01f) {
		// Moving or in vehicle: advance animation time in LegoTime units (ms-scale)
		// Game uses: m_actorTime += deltaTime_ms * worldSpeed (see legopathactor.cpp:359)
		// When on a vehicle but standing still, freeze at frame 0
		if (m_targetSpeed > 0.01f) {
			m_animTime += p_deltaTime * 2000.0f;
		}
		float duration = (float) anim->GetDuration();
		if (duration > 0.0f) {
			float timeInCycle = m_animTime - duration * floorf(m_animTime / duration);

			MxMatrix transform(m_roi->GetLocal2World());
			LegoTreeNode* root = anim->GetRoot();
			for (LegoU32 i = 0; i < root->GetNumChildren(); i++) {
				LegoROI::ApplyAnimationTransformation(root->GetChild(i), transform, (LegoTime) timeInCycle, roiMap);
			}
		}
		m_wasMoving = true;
		m_idleTime = 0.0f;
		m_idleAnimTime = 0.0f;
	}
	else if (m_idleAnim && m_idleRoiMap) {
		// Standing still on foot: use the dedicated idle animation (CNs008xx)
		if (m_wasMoving) {
			m_wasMoving = false;
			m_idleTime = 0.0f;
			m_idleAnimTime = 0.0f;
		}

		m_idleTime += p_deltaTime;

		// Play idle animation: frame 0 for first 2.5s (standing pose),
		// then continuously loop (breathing/swaying effect)
		if (m_idleTime >= 2.5f) {
			m_idleAnimTime += p_deltaTime * 1000.0f;
		}

		float duration = (float) m_idleAnim->GetDuration();
		if (duration > 0.0f) {
			float timeInCycle = m_idleAnimTime - duration * floorf(m_idleAnimTime / duration);

			MxMatrix transform(m_roi->GetLocal2World());
			LegoTreeNode* root = m_idleAnim->GetRoot();
			for (LegoU32 i = 0; i < root->GetNumChildren(); i++) {
				LegoROI::ApplyAnimationTransformation(root->GetChild(i), transform, (LegoTime) timeInCycle, m_idleRoiMap);
			}
		}
	}
}

void RemotePlayer::UpdateVehicleState()
{
	if (m_targetVehicleType != m_currentVehicleType) {
		if (m_targetVehicleType == VEHICLE_NONE) {
			// Exiting vehicle
			ExitVehicle();
		}
		else {
			// Entering vehicle (exit old one first if needed)
			if (m_currentVehicleType != VEHICLE_NONE) {
				ExitVehicle();
			}
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
		// Large vehicle: hide character, show vehicle ROI
		m_roi->SetVisibility(FALSE);

		// Create vehicle ROI clone
		char vehicleName[48];
		SDL_snprintf(vehicleName, sizeof(vehicleName), "%s_mp_%u", g_vehicleROINames[p_vehicleType], m_peerId);

		m_vehicleROI = CharacterManager()->CreateAutoROI(vehicleName, g_vehicleROINames[p_vehicleType], FALSE);
		if (m_vehicleROI) {
			// CreateAutoROI already adds to 3D scene via Get3DManager()->Add()
			// Position at current transform
			MxMatrix mat(m_roi->GetLocal2World());
			m_vehicleROI->WrappedSetLocal2WorldWithWorldDataUpdate(mat);
			m_vehicleROI->SetVisibility(m_visible ? TRUE : FALSE);
		}
		else {
			SDL_Log("Multiplayer: failed to create vehicle ROI for type %d", p_vehicleType);
		}
	}
	else {
		// Small vehicle: find ride animation and build ride ROI map
		const char* rideAnimName = g_rideAnimNames[p_vehicleType];
		const char* vehicleVariantName = g_rideVehicleROINames[p_vehicleType];

		if (!rideAnimName || !vehicleVariantName) {
			return;
		}

		// Find the ride animation presenter
		LegoWorld* world = CurrentWorld();
		if (!world) {
			return;
		}

		MxCore* presenter = world->Find("LegoAnimPresenter", rideAnimName);
		if (!presenter) {
			SDL_Log("Multiplayer: ride animation presenter %s not found", rideAnimName);
			return;
		}

		LegoAnimPresenter* animPresenter = static_cast<LegoAnimPresenter*>(presenter);
		m_rideAnim = animPresenter->GetAnimation();
		if (!m_rideAnim) {
			SDL_Log("Multiplayer: ride animation data is null for %s", rideAnimName);
			return;
		}

		// Create vehicle variant ROI for the ride animation
		char variantName[48];
		SDL_snprintf(variantName, sizeof(variantName), "%s_mp_%u", vehicleVariantName, m_peerId);
		m_rideVehicleROI = CharacterManager()->CreateAutoROI(variantName, vehicleVariantName, FALSE);
		// CreateAutoROI already adds to 3D scene via Get3DManager()->Add()

		// Rename to base name so FindChildROI in AssignROIIndices can match animation tree nodes.
		// CreateAutoROI sets name to unique variantName (e.g. "board_mp_2") but animation nodes
		// expect the base name (e.g. "board"). ReleaseAutoROI uses pointer comparison, not name.
		if (m_rideVehicleROI) {
			m_rideVehicleROI->SetName(vehicleVariantName);
		}

		// Build the ride ROI map with both character body parts and vehicle variant
		BuildROIMap(m_rideAnim, m_roi, m_rideVehicleROI, m_rideRoiMap, m_rideRoiMapSize);
	}
}

void RemotePlayer::ExitVehicle()
{
	if (m_currentVehicleType == VEHICLE_NONE) {
		return;
	}

	// Clean up large vehicle ROI
	if (m_vehicleROI) {
		VideoManager()->Get3DManager()->Remove(*m_vehicleROI);
		CharacterManager()->ReleaseAutoROI(m_vehicleROI);
		m_vehicleROI = nullptr;
	}

	// Clean up ride animation state
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

	// Show character again
	if (m_visible) {
		m_roi->SetVisibility(TRUE);
	}

	m_currentVehicleType = VEHICLE_NONE;
	m_animTime = 0.0f;
	m_wasMoving = false;
}
