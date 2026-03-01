#include "extensions/multiplayer/charactercloner.h"

#include "legoactors.h"
#include "legocharactermanager.h"
#include "legovideomanager.h"
#include "misc.h"
#include "misc/legocontainer.h"
#include "realtime/realtime.h"
#include "roi/legolod.h"
#include "roi/legoroi.h"
#include "viewmanager/viewlodlist.h"

#include <SDL3/SDL_stdinc.h>
#include <cstdio>
#include <vec.h>

using namespace Multiplayer;

LegoROI* CharacterCloner::Clone(LegoCharacterManager* p_charMgr, const char* p_uniqueName, const char* p_characterType)
{
	MxBool success = FALSE;
	LegoROI* roi = NULL;
	BoundingSphere boundingSphere;
	BoundingBox boundingBox;
	MxMatrix mat;
	CompoundObject* comp;
	MxS32 i;

	Tgl::Renderer* renderer = VideoManager()->GetRenderer();
	ViewLODListManager* lodManager = GetViewLODListManager();
	LegoTextureContainer* textureContainer = TextureContainer();
	LegoActorInfo* info = p_charMgr->GetActorInfo(p_characterType);

	if (info == NULL) {
		goto done;
	}

	roi = new LegoROI(renderer);
	roi->SetName(p_uniqueName);

	boundingSphere.Center()[0] = g_actorLODs[c_topLOD].m_boundingSphere[0];
	boundingSphere.Center()[1] = g_actorLODs[c_topLOD].m_boundingSphere[1];
	boundingSphere.Center()[2] = g_actorLODs[c_topLOD].m_boundingSphere[2];
	boundingSphere.Radius() = g_actorLODs[c_topLOD].m_boundingSphere[3];
	roi->SetBoundingSphere(boundingSphere);

	boundingBox.Min()[0] = g_actorLODs[c_topLOD].m_boundingBox[0];
	boundingBox.Min()[1] = g_actorLODs[c_topLOD].m_boundingBox[1];
	boundingBox.Min()[2] = g_actorLODs[c_topLOD].m_boundingBox[2];
	boundingBox.Max()[0] = g_actorLODs[c_topLOD].m_boundingBox[3];
	boundingBox.Max()[1] = g_actorLODs[c_topLOD].m_boundingBox[4];
	boundingBox.Max()[2] = g_actorLODs[c_topLOD].m_boundingBox[5];
	roi->SetBoundingBox(boundingBox);

	comp = new CompoundObject();
	roi->SetComp(comp);

	for (i = 0; i < sizeOfArray(g_actorLODs) - 1; i++) {
		char lodName[256];
		LegoActorInfo::Part& part = info->m_parts[i];

		const char* parentName;
		if (i == 0 || i == 1) {
			parentName = part.m_partName[part.m_partNameIndices[part.m_partNameIndex]];
		}
		else {
			parentName = g_actorLODs[i + 1].m_parentName;
		}

		ViewLODList* lodList = lodManager->Lookup(parentName);
		MxS32 lodSize = lodList->Size();
		sprintf(lodName, "%s%d", p_uniqueName, i);
		ViewLODList* dupLodList = lodManager->Create(lodName, lodSize);

		for (MxS32 j = 0; j < lodSize; j++) {
			LegoLOD* lod = (LegoLOD*) (*lodList)[j];
			LegoLOD* clone = lod->Clone(renderer);
			dupLodList->PushBack(clone);
		}

		lodList->Release();
		lodList = dupLodList;

		LegoROI* childROI = new LegoROI(renderer, lodList);
		lodList->Release();

		childROI->SetName(g_actorLODs[i + 1].m_name);
		childROI->SetParentROI(roi);

		BoundingSphere childBoundingSphere;
		childBoundingSphere.Center()[0] = g_actorLODs[i + 1].m_boundingSphere[0];
		childBoundingSphere.Center()[1] = g_actorLODs[i + 1].m_boundingSphere[1];
		childBoundingSphere.Center()[2] = g_actorLODs[i + 1].m_boundingSphere[2];
		childBoundingSphere.Radius() = g_actorLODs[i + 1].m_boundingSphere[3];
		childROI->SetBoundingSphere(childBoundingSphere);

		BoundingBox childBoundingBox;
		childBoundingBox.Min()[0] = g_actorLODs[i + 1].m_boundingBox[0];
		childBoundingBox.Min()[1] = g_actorLODs[i + 1].m_boundingBox[1];
		childBoundingBox.Min()[2] = g_actorLODs[i + 1].m_boundingBox[2];
		childBoundingBox.Max()[0] = g_actorLODs[i + 1].m_boundingBox[3];
		childBoundingBox.Max()[1] = g_actorLODs[i + 1].m_boundingBox[4];
		childBoundingBox.Max()[2] = g_actorLODs[i + 1].m_boundingBox[5];
		childROI->SetBoundingBox(childBoundingBox);

		CalcLocalTransform(
			Mx3DPointFloat(g_actorLODs[i + 1].m_position),
			Mx3DPointFloat(g_actorLODs[i + 1].m_direction),
			Mx3DPointFloat(g_actorLODs[i + 1].m_up),
			mat
		);
		childROI->WrappedSetLocal2WorldWithWorldDataUpdate(mat);

		if (g_actorLODs[i + 1].m_flags & LegoActorLOD::c_useTexture &&
			(i != 0 || part.m_partNameIndices[part.m_partNameIndex] != 0)) {

			LegoTextureInfo* textureInfo = textureContainer->Get(part.m_names[part.m_nameIndices[part.m_nameIndex]]);

			if (textureInfo != NULL) {
				childROI->SetTextureInfo(textureInfo);
				childROI->SetLodColor(1.0F, 1.0F, 1.0F, 0.0F);
			}
		}
		else if (g_actorLODs[i + 1].m_flags & LegoActorLOD::c_useColor || (i == 0 && part.m_partNameIndices[part.m_partNameIndex] == 0)) {
			LegoFloat red, green, blue, alpha;
			childROI->GetRGBAColor(part.m_names[part.m_nameIndices[part.m_nameIndex]], red, green, blue, alpha);
			childROI->SetLodColor(red, green, blue, alpha);
		}

		comp->push_back(childROI);
	}

	CalcLocalTransform(
		Mx3DPointFloat(g_actorLODs[c_topLOD].m_position),
		Mx3DPointFloat(g_actorLODs[c_topLOD].m_direction),
		Mx3DPointFloat(g_actorLODs[c_topLOD].m_up),
		mat
	);
	roi->WrappedSetLocal2WorldWithWorldDataUpdate(mat);

	{
		LegoCharacter* character = new LegoCharacter(roi);
		char* name = new char[SDL_strlen(p_uniqueName) + 1];
		SDL_strlcpy(name, p_uniqueName, SDL_strlen(p_uniqueName) + 1);
		(*p_charMgr->m_characters)[name] = character;
	}

	success = TRUE;

done:
	if (!success && roi != NULL) {
		delete roi;
		roi = NULL;
	}

	return roi;
}
