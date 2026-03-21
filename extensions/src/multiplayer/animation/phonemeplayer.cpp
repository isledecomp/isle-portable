#include "extensions/multiplayer/animation/phonemeplayer.h"

#include "extensions/multiplayer/animation/loader.h"
#include "flic.h"
#include "legocharactermanager.h"
#include "misc.h"
#include "misc/legocontainer.h"
#include "mxbitmap.h"
#include "roi/legoroi.h"

#include <SDL3/SDL_stdinc.h>

using namespace Multiplayer::Animation;

// Find the ROI matching a phoneme track's roiName in the roiMap.
static LegoROI* FindTrackROI(const std::string& p_roiName, LegoROI** p_roiMap, MxU32 p_roiMapSize)
{
	if (p_roiName.empty() || !p_roiMap) {
		return nullptr;
	}

	for (MxU32 i = 1; i < p_roiMapSize; i++) {
		if (p_roiMap[i] && p_roiMap[i]->GetName() && !SDL_strcasecmp(p_roiName.c_str(), p_roiMap[i]->GetName())) {
			return p_roiMap[i];
		}
	}
	return nullptr;
}

void PhonemePlayer::Init(const std::vector<SceneAnimData::PhonemeTrack>& p_tracks, LegoROI** p_roiMap, MxU32 p_roiMapSize)
{
	for (auto& track : p_tracks) {
		PhonemeState state;
		state.targetROI = nullptr;
		state.originalTexture = nullptr;
		state.cachedTexture = nullptr;
		state.bitmap = nullptr;
		state.currentFrame = -1;

		// Resolve the target ROI from the track's roiName via the roiMap
		LegoROI* targetROI = FindTrackROI(track.roiName, p_roiMap, p_roiMapSize);
		if (!targetROI) {
			m_states.push_back(state);
			continue;
		}
		state.targetROI = targetROI;

		LegoROI* head = targetROI->FindChildROI("head", targetROI);
		if (!head) {
			m_states.push_back(state);
			continue;
		}

		LegoTextureInfo* originalInfo = nullptr;
		head->GetTextureInfo(originalInfo);
		if (!originalInfo) {
			m_states.push_back(state);
			continue;
		}
		state.originalTexture = originalInfo;

		LegoTextureInfo* cached = TextureContainer()->GetCached(originalInfo);
		if (!cached) {
			m_states.push_back(state);
			continue;
		}
		state.cachedTexture = cached;

		CharacterManager()->SetHeadTexture(targetROI, cached);

		state.bitmap = new MxBitmap();
		state.bitmap->SetSize(track.width, track.height, NULL, FALSE);

		m_states.push_back(state);
	}
}

void PhonemePlayer::Tick(float p_elapsedMs, const std::vector<SceneAnimData::PhonemeTrack>& p_tracks)
{
	for (size_t i = 0; i < p_tracks.size() && i < m_states.size(); i++) {
		auto& track = p_tracks[i];
		auto& state = m_states[i];

		if (!state.bitmap || !state.cachedTexture) {
			continue;
		}

		float trackElapsed = p_elapsedMs - (float) track.timeOffset;
		if (trackElapsed < 0.0f) {
			continue;
		}

		if (track.flcHeader->speed == 0) {
			continue;
		}

		int targetFrame = (int) (trackElapsed / (float) track.flcHeader->speed);
		if (targetFrame == state.currentFrame) {
			continue;
		}
		if (targetFrame >= (int) track.frameData.size()) {
			continue;
		}

		int startFrame = state.currentFrame + 1;
		if (startFrame < 0) {
			startFrame = 0;
		}

		for (int f = startFrame; f <= targetFrame; f++) {
			const auto& data = track.frameData[f];
			if (data.size() < sizeof(MxS32)) {
				continue;
			}

			MxS32 rectCount;
			SDL_memcpy(&rectCount, data.data(), sizeof(MxS32));
			size_t headerSize = sizeof(MxS32) + rectCount * sizeof(MxRect32);
			if (data.size() <= headerSize) {
				continue;
			}

			FLIC_FRAME* flcFrame = (FLIC_FRAME*) (data.data() + headerSize);

			BYTE decodedColorMap;
			DecodeFLCFrame(
				&state.bitmap->GetBitmapInfo()->m_bmiHeader,
				state.bitmap->GetImage(),
				track.flcHeader,
				flcFrame,
				&decodedColorMap
			);

			// When the FLC frame updates the palette, apply it to the texture surface
			if (decodedColorMap && state.cachedTexture->m_palette) {
				PALETTEENTRY entries[256];
				RGBQUAD* colors = state.bitmap->GetBitmapInfo()->m_bmiColors;
				for (int c = 0; c < 256; c++) {
					entries[c].peRed = colors[c].rgbRed;
					entries[c].peGreen = colors[c].rgbGreen;
					entries[c].peBlue = colors[c].rgbBlue;
					entries[c].peFlags = PC_NONE;
				}
				state.cachedTexture->m_palette->SetEntries(0, 0, 256, entries);
			}
		}

		state.cachedTexture->LoadBits(state.bitmap->GetImage());
		state.currentFrame = targetFrame;
	}
}

void PhonemePlayer::Cleanup()
{
	for (size_t i = 0; i < m_states.size(); i++) {
		auto& state = m_states[i];

		if (state.targetROI && state.originalTexture) {
			CharacterManager()->SetHeadTexture(state.targetROI, state.originalTexture);
		}

		if (state.cachedTexture) {
			TextureContainer()->EraseCached(state.cachedTexture);
		}

		delete state.bitmap;
	}
	m_states.clear();
}
