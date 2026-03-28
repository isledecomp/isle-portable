#pragma once

#include "extensions/multiplayer/animation/audioplayer.h"
#include "extensions/multiplayer/animation/catalog.h"
#include "extensions/multiplayer/animation/loader.h"
#include "extensions/multiplayer/animation/phonemeplayer.h"
#include "mxgeometry/mxmatrix.h"
#include "mxtypes.h"

#include <cstdint>
#include <string>
#include <vector>

class LegoROI;
struct AnimInfo;

namespace Multiplayer::Animation
{

// A participant (local or remote player) whose ROI is borrowed during animation
struct ParticipantROI {
	LegoROI* roi;
	LegoROI* vehicleROI; // Ride vehicle ROI (bike/board/moto), or nullptr
	MxMatrix savedTransform;
	std::string savedName;
	int8_t charIndex; // g_characters[] index, or -1 for spectator

	bool IsSpectator() const { return charIndex < 0; }
};

class ScenePlayer {
public:
	ScenePlayer();
	~ScenePlayer();

	// When p_observerMode is false, p_participants[0] must be the local player.
	// When p_observerMode is true, participants are only remote performers (no local player).
	void Play(
		const AnimInfo* p_animInfo,
		AnimCategory p_category,
		const ParticipantROI* p_participants,
		uint8_t p_participantCount,
		bool p_observerMode = false
	);
	void Tick();
	void Stop();
	bool IsPlaying() const { return m_playing; }
	bool IsObserverMode() const { return m_observerMode; }

	void SetLoader(Loader* p_loader) { m_loader = p_loader; }

private:
	void ComputeRebaseMatrix();
	void SetupROIs(const AnimInfo* p_animInfo);
	void ResolvePtAtCamROIs();
	void ApplyPtAtCam();
	void CleanupProps();

	// Sub-components
	Loader* m_loader;
	AudioPlayer m_audioPlayer;
	PhonemePlayer m_phonemePlayer;

	// Playback state
	bool m_playing;
	bool m_rebaseComputed;
	uint64_t m_startTime;
	SceneAnimData* m_currentData;
	AnimCategory m_category;
	MxMatrix m_animPose0;
	MxMatrix m_rebaseMatrix;

	// Participants (local player at index 0, remote players after)
	std::vector<ParticipantROI> m_participants;

	// Root performer ROI (rebase anchor for NPC anims)
	LegoROI* m_animRootROI;

	// Vehicle ROI borrowed from a participant during playback
	LegoROI* m_vehicleROI;

	// Player's ride vehicle hidden during cam_anim (not borrowed, just hidden)
	LegoROI* m_hiddenVehicleROI;

	// ROI map for skeletal animation
	LegoROI** m_roiMap;
	MxU32 m_roiMapSize;

	// Actor name → ROI aliases (participant ROIs whose names differ from animation actor names)
	std::vector<std::pair<std::string, LegoROI*>> m_actorAliases;

	// Props created for the animation (cloned characters and prop models)
	std::vector<LegoROI*> m_propROIs;

	// ROIs cloned from scene (created by sharing LOD data, not registered in CharacterManager)
	std::vector<LegoROI*> m_clonedSceneROIs;

	bool m_hasCamAnim;
	bool m_observerMode;
	std::vector<LegoROI*> m_ptAtCamROIs;
	bool m_hideOnStop;
};

} // namespace Multiplayer::Animation
