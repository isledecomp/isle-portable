#include "extensions/thirdpersoncamera/displayactor.h"

#include "3dmanager/lego3dmanager.h"
#include "extensions/common/animutils.h"
#include "extensions/common/charactercloner.h"
#include "extensions/common/charactercustomizer.h"
#include "extensions/common/constants.h"
#include "legocharactermanager.h"
#include "legovideomanager.h"
#include "misc.h"
#include "mxgeometry/mxmatrix.h"
#include "realtime/vector.h"
#include "roi/legoroi.h"

#include <SDL3/SDL_stdinc.h>

using namespace Extensions::ThirdPersonCamera;
using namespace Extensions::Common;

DisplayActor::DisplayActor()
	: m_displayActorIndex(DISPLAY_ACTOR_NONE), m_displayActorFrozen(false), m_displayROI(nullptr)
{
	SDL_memset(m_displayUniqueName, 0, sizeof(m_displayUniqueName));
}

void DisplayActor::SetDisplayActorIndex(uint8_t p_displayActorIndex)
{
	if (m_displayActorIndex != p_displayActorIndex) {
		m_customizeState.InitFromActorInfo(p_displayActorIndex);
	}
	m_displayActorIndex = p_displayActorIndex;
}

bool DisplayActor::EnsureDisplayROI()
{
	if (!IsValidDisplayActorIndex(m_displayActorIndex)) {
		return false;
	}
	if (!m_displayROI) {
		CreateDisplayClone();
	}
	if (!m_displayROI) {
		return false;
	}
	return true;
}

void DisplayActor::CreateDisplayClone()
{
	if (!IsValidDisplayActorIndex(m_displayActorIndex)) {
		return;
	}
	LegoCharacterManager* charMgr = CharacterManager();
	const char* actorName = charMgr->GetActorName(m_displayActorIndex);
	if (!actorName) {
		return;
	}
	SDL_snprintf(m_displayUniqueName, sizeof(m_displayUniqueName), "tp_display");
	m_displayROI = CharacterCloner::Clone(charMgr, m_displayUniqueName, actorName);

	if (m_displayROI) {
		CharacterCustomizer::ApplyFullState(m_displayROI, m_displayActorIndex, m_customizeState);
	}
}

void DisplayActor::DestroyDisplayClone()
{
	if (m_displayROI) {
		VideoManager()->Get3DManager()->Remove(*m_displayROI);
		CharacterManager()->ReleaseActor(m_displayUniqueName);
		m_displayROI = nullptr;
	}
}

void DisplayActor::ApplyCustomizeChange(uint8_t p_changeType, uint8_t p_partIndex)
{
	uint8_t actorInfoIndex = CharacterCustomizer::ResolveActorInfoIndex(m_displayActorIndex);

	CharacterCustomizer::ApplyChange(m_displayROI, actorInfoIndex, m_customizeState, p_changeType, p_partIndex);
}

void DisplayActor::SyncTransformFromNative(LegoROI* p_nativeROI)
{
	if (m_displayROI && p_nativeROI) {
		MxMatrix mat(p_nativeROI->GetLocal2World());
		AnimUtils::FlipMatrixDirection(mat);
		m_displayROI->WrappedSetLocal2WorldWithWorldDataUpdate(mat);
		VideoManager()->Get3DManager()->Moved(*m_displayROI);
	}
}
