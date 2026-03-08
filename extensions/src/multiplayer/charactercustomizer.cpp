#include "extensions/multiplayer/charactercustomizer.h"

#include "extensions/multiplayer/charactercloner.h"
#include "extensions/multiplayer/customizestate.h"
#include "extensions/multiplayer/protocol.h"

#include "3dmanager/lego3dmanager.h"
#include "3dmanager/lego3dview.h"
#include "legoactors.h"
#include "legocharactermanager.h"
#include "legovideomanager.h"
#include "misc.h"
#include "mxatom.h"
#include "mxdsaction.h"
#include "mxmisc.h"
#include "roi/legolod.h"
#include "roi/legoroi.h"
#include "viewmanager/viewlodlist.h"
#include "viewmanager/viewmanager.h"

#include <SDL3/SDL_stdinc.h>
#include <cstdio>

using namespace Multiplayer;

static const MxU32 g_characterSoundIdOffset = 50;
static const MxU32 g_characterSoundIdMoodOffset = 66;
static const MxU32 g_characterAnimationId = 10;
static const MxU32 g_maxSound = 9;
static const MxU32 g_maxMove = 4;

static uint32_t s_variantCounter = 10000;

// MARK: Private helpers

LegoROI* CharacterCustomizer::FindChildROI(LegoROI* p_rootROI, const char* p_name)
{
	const CompoundObject* comp = p_rootROI->GetComp();

	for (CompoundObject::const_iterator it = comp->begin(); it != comp->end(); it++) {
		LegoROI* roi = (LegoROI*) *it;

		if (!SDL_strcasecmp(p_name, roi->GetName())) {
			return roi;
		}
	}

	return NULL;
}

// MARK: Public API

uint8_t CharacterCustomizer::ResolveActorInfoIndex(uint8_t p_displayActorIndex)
{
	return p_displayActorIndex;
}

bool CharacterCustomizer::SwitchColor(
	LegoROI* p_rootROI,
	uint8_t p_actorInfoIndex,
	CustomizeState& p_state,
	int p_partIndex
)
{
	if (p_partIndex < 0 || p_partIndex >= 10) {
		return false;
	}

	// Remap derived parts to independent parts
	if (p_partIndex == c_clawlftPart) {
		p_partIndex = c_armlftPart;
	}
	else if (p_partIndex == c_clawrtPart) {
		p_partIndex = c_armrtPart;
	}
	else if (p_partIndex == c_headPart) {
		p_partIndex = c_infohatPart;
	}
	else if (p_partIndex == c_bodyPart) {
		p_partIndex = c_infogronPart;
	}

	if (!(g_actorLODs[p_partIndex + 1].m_flags & LegoActorLOD::c_useColor)) {
		return false;
	}

	if (p_actorInfoIndex >= sizeOfArray(g_actorInfoInit)) {
		return false;
	}

	const LegoActorInfo::Part& part = g_actorInfoInit[p_actorInfoIndex].m_parts[p_partIndex];

	p_state.colorIndices[p_partIndex]++;
	if (part.m_nameIndices[p_state.colorIndices[p_partIndex]] == 0xff) {
		p_state.colorIndices[p_partIndex] = 0;
	}

	if (!p_rootROI) {
		return true;
	}

	LegoROI* targetROI = FindChildROI(p_rootROI, g_actorLODs[p_partIndex + 1].m_name);
	if (!targetROI) {
		return false;
	}

	LegoFloat red, green, blue, alpha;
	LegoROI::GetRGBAColor(part.m_names[part.m_nameIndices[p_state.colorIndices[p_partIndex]]], red, green, blue, alpha);
	targetROI->SetLodColor(red, green, blue, alpha);
	return true;
}

bool CharacterCustomizer::SwitchVariant(LegoROI* p_rootROI, uint8_t p_actorInfoIndex, CustomizeState& p_state)
{
	if (p_actorInfoIndex >= sizeOfArray(g_actorInfoInit)) {
		return false;
	}

	const LegoActorInfo::Part& part = g_actorInfoInit[p_actorInfoIndex].m_parts[c_infohatPart];

	p_state.hatVariantIndex++;
	if (part.m_partNameIndices[p_state.hatVariantIndex] == 0xff) {
		p_state.hatVariantIndex = 0;
	}

	if (!p_rootROI) {
		return true;
	}

	ApplyHatVariant(p_rootROI, p_actorInfoIndex, p_state);
	return true;
}

bool CharacterCustomizer::SwitchSound(CustomizeState& p_state)
{
	p_state.sound++;
	if (p_state.sound >= g_maxSound) {
		p_state.sound = 0;
	}
	return true;
}

bool CharacterCustomizer::SwitchMove(CustomizeState& p_state)
{
	p_state.move++;
	if (p_state.move >= g_maxMove) {
		p_state.move = 0;
	}
	return true;
}

bool CharacterCustomizer::SwitchMood(CustomizeState& p_state)
{
	p_state.mood++;
	if (p_state.mood > 3) {
		p_state.mood = 0;
	}
	return true;
}

void CharacterCustomizer::ApplyChange(
	LegoROI* p_rootROI,
	uint8_t p_actorInfoIndex,
	CustomizeState& p_state,
	uint8_t p_changeType,
	uint8_t p_partIndex
)
{
	switch (p_changeType) {
	case CHANGE_VARIANT:
		SwitchVariant(p_rootROI, p_actorInfoIndex, p_state);
		break;
	case CHANGE_SOUND:
		SwitchSound(p_state);
		break;
	case CHANGE_MOVE:
		SwitchMove(p_state);
		break;
	case CHANGE_COLOR:
		SwitchColor(p_rootROI, p_actorInfoIndex, p_state, p_partIndex);
		break;
	case CHANGE_MOOD:
		SwitchMood(p_state);
		break;
	}
}

int CharacterCustomizer::MapClickedPartIndex(const char* p_partName)
{
	for (int i = 0; i < 10; i++) {
		if (!SDL_strcasecmp(p_partName, g_actorLODs[i + 1].m_name)) {
			return i;
		}
	}
	return -1;
}

void CharacterCustomizer::ApplyFullState(
	LegoROI* p_rootROI,
	uint8_t p_actorInfoIndex,
	const CustomizeState& p_state
)
{
	if (p_actorInfoIndex >= sizeOfArray(g_actorInfoInit)) {
		return;
	}

	// Apply colors for the 6 independent colorable parts
	static const int colorableParts[] = {
		c_infohatPart, c_infogronPart, c_armlftPart, c_armrtPart, c_leglftPart, c_legrtPart
	};

	for (int i = 0; i < (int) sizeOfArray(colorableParts); i++) {
		int partIndex = colorableParts[i];

		if (!(g_actorLODs[partIndex + 1].m_flags & LegoActorLOD::c_useColor)) {
			continue;
		}

		LegoROI* childROI = FindChildROI(p_rootROI, g_actorLODs[partIndex + 1].m_name);
		if (!childROI) {
			continue;
		}

		const LegoActorInfo::Part& part = g_actorInfoInit[p_actorInfoIndex].m_parts[partIndex];

		LegoFloat red, green, blue, alpha;
		LegoROI::GetRGBAColor(
			part.m_names[part.m_nameIndices[p_state.colorIndices[partIndex]]],
			red,
			green,
			blue,
			alpha
		);
		childROI->SetLodColor(red, green, blue, alpha);
	}

	// Apply hat variant if different from default
	const LegoActorInfo::Part& hatPart = g_actorInfoInit[p_actorInfoIndex].m_parts[c_infohatPart];
	if (p_state.hatVariantIndex != hatPart.m_partNameIndex) {
		ApplyHatVariant(p_rootROI, p_actorInfoIndex, p_state);
	}
}

void CharacterCustomizer::ApplyHatVariant(
	LegoROI* p_rootROI,
	uint8_t p_actorInfoIndex,
	const CustomizeState& p_state
)
{
	if (p_actorInfoIndex >= sizeOfArray(g_actorInfoInit)) {
		return;
	}

	const LegoActorInfo::Part& part = g_actorInfoInit[p_actorInfoIndex].m_parts[c_infohatPart];

	MxU8 partNameIndex = part.m_partNameIndices[p_state.hatVariantIndex];
	if (partNameIndex == 0xff) {
		return;
	}

	LegoROI* childROI = FindChildROI(p_rootROI, g_actorLODs[c_infohatLOD].m_name);

	if (childROI != NULL) {
		char lodName[256];

		ViewLODList* lodList = GetViewLODListManager()->Lookup(part.m_partName[partNameIndex]);
		MxS32 lodSize = lodList->Size();
		sprintf(lodName, "%s_cv%u", p_rootROI->GetName(), s_variantCounter++);
		ViewLODList* dupLodList = GetViewLODListManager()->Create(lodName, lodSize);

		Tgl::Renderer* renderer = VideoManager()->GetRenderer();
		LegoFloat red, green, blue, alpha;
		LegoROI::GetRGBAColor(
			part.m_names[part.m_nameIndices[p_state.colorIndices[c_infohatPart]]],
			red,
			green,
			blue,
			alpha
		);

		for (MxS32 i = 0; i < lodSize; i++) {
			LegoLOD* lod = (LegoLOD*) (*lodList)[i];
			LegoLOD* clone = lod->Clone(renderer);
			clone->SetColor(red, green, blue, alpha);
			dupLodList->PushBack(clone);
		}

		lodList->Release();
		lodList = dupLodList;

		if (childROI->GetLodLevel() >= 0) {
			VideoManager()->Get3DManager()->GetLego3DView()->GetViewManager()->RemoveROIDetailFromScene(childROI);
		}

		childROI->SetLODList(lodList);
		lodList->Release();
	}
}

void CharacterCustomizer::PlayClickSound(LegoROI* p_roi, const CustomizeState& p_state, bool p_basedOnMood)
{
	MxU32 objectId = p_basedOnMood ? (p_state.mood + g_characterSoundIdMoodOffset)
	                               : (p_state.sound + g_characterSoundIdOffset);

	if (objectId) {
		MxDSAction action;
		action.SetAtomId(MxAtomId(LegoCharacterManager::GetCustomizeAnimFile(), e_lowerCase2));
		action.SetObjectId(objectId);

		const char* name = p_roi->GetName();
		action.AppendExtra(SDL_strlen(name) + 1, name);
		Start(&action);
	}
}

MxU32 CharacterCustomizer::PlayClickAnimation(LegoROI* p_roi, const CustomizeState& p_state)
{
	MxU32 objectId = p_state.move + g_characterAnimationId;

	MxDSAction action;
	action.SetAtomId(MxAtomId(LegoCharacterManager::GetCustomizeAnimFile(), e_lowerCase2));
	action.SetObjectId(objectId);

	char extra[1024];
	SDL_snprintf(extra, sizeof(extra), "SUBST:actor_01:%s", p_roi->GetName());
	action.AppendExtra(SDL_strlen(extra) + 1, extra);
	StartActionIfInitialized(action);

	return objectId;
}

void CharacterCustomizer::StopClickAnimation(MxU32 p_objectId)
{
	MxDSAction action;
	action.SetAtomId(MxAtomId(LegoCharacterManager::GetCustomizeAnimFile(), e_lowerCase2));
	action.SetObjectId(p_objectId);
	DeleteObject(action);
}
