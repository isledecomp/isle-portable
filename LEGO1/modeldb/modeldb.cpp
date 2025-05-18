#include "modeldb.h"

DECOMP_SIZE_ASSERT(ModelDbWorld, 0x18)
DECOMP_SIZE_ASSERT(ModelDbPart, 0x18)
DECOMP_SIZE_ASSERT(ModelDbModel, 0x38)
DECOMP_SIZE_ASSERT(ModelDbPartList, 0x1c)
DECOMP_SIZE_ASSERT(ModelDbPartListCursor, 0x10)

// FUNCTION: LEGO1 0x10027690
// FUNCTION: BETA10 0x100e5620
void ModelDbModel::Free()
{
	delete[] m_modelName;
	delete[] m_presenterName;
}

// FUNCTION: LEGO1 0x100276b0
MxResult ModelDbModel::Read(SDL_IOStream* p_file)
{
	MxU32 len;

	if (SDL_ReadIO(p_file, &len, sizeof(MxU32)) != sizeof(MxU32)) {
		return FAILURE;
	}

	m_modelName = new char[len];
	if (SDL_ReadIO(p_file, m_modelName, len) != len) {
		return FAILURE;
	}

	if (SDL_ReadIO(p_file, &m_modelDataLength, sizeof(MxU32)) != sizeof(MxU32)) {
		return FAILURE;
	}
	if (SDL_ReadIO(p_file, &m_modelDataOffset, sizeof(MxU32)) != sizeof(MxU32)) {
		return FAILURE;
	}
	if (SDL_ReadIO(p_file, &len, sizeof(len)) != sizeof(len)) {
		return FAILURE;
	}

	m_presenterName = new char[len];
	if (SDL_ReadIO(p_file, m_presenterName, len) != len) {
		return FAILURE;
	}

	if (SDL_ReadIO(p_file, m_location, 3 * sizeof(float)) != 3 * sizeof(float)) {
		return FAILURE;
	}
	if (SDL_ReadIO(p_file, m_direction, 3 * sizeof(float)) != 3 * sizeof(float)) {
		return FAILURE;
	}
	if (SDL_ReadIO(p_file, m_up, 3 * sizeof(float)) != 3 * sizeof(float)) {
		return FAILURE;
	}
	if (SDL_ReadIO(p_file, &m_unk0x34, sizeof(undefined)) != sizeof(undefined)) {
		return FAILURE;
	}

	return SUCCESS;
}

// FUNCTION: LEGO1 0x10027850
MxResult ModelDbPart::Read(SDL_IOStream* p_file)
{
	MxU32 len;

	if (SDL_ReadIO(p_file, &len, sizeof(MxU32)) != sizeof(MxU32)) {
		return FAILURE;
	}

	char* buff = new char[len];

	if (SDL_ReadIO(p_file, buff, len) != len) {
		return FAILURE;
	}

	m_roiName = buff;
	delete[] buff;

	if (SDL_ReadIO(p_file, &m_partDataLength, sizeof(undefined4)) != sizeof(undefined4)) {
		return FAILURE;
	}
	if (SDL_ReadIO(p_file, &m_partDataOffset, sizeof(undefined4)) != sizeof(undefined4)) {
		return FAILURE;
	}

	return SUCCESS;
}

// FUNCTION: LEGO1 0x10027910
MxResult ReadModelDbWorlds(SDL_IOStream* p_file, ModelDbWorld*& p_worlds, MxS32& p_numWorlds)
{
	p_worlds = NULL;
	p_numWorlds = 0;

	MxS32 numWorlds;
	if (SDL_ReadIO(p_file, &numWorlds, sizeof(numWorlds)) != sizeof(numWorlds)) {
		return FAILURE;
	}

	ModelDbWorld* worlds = new ModelDbWorld[numWorlds];
	MxS32 worldNameLen, numParts, i, j;

	for (i = 0; i < numWorlds; i++) {
		if (SDL_ReadIO(p_file, &worldNameLen, sizeof(MxS32)) != sizeof(MxS32)) {
			return FAILURE;
		}

		worlds[i].m_worldName = new char[worldNameLen];
		if (SDL_ReadIO(p_file, worlds[i].m_worldName, worldNameLen) != worldNameLen) {
			return FAILURE;
		}

		if (SDL_ReadIO(p_file, &numParts, sizeof(MxS32)) != sizeof(MxS32)) {
			return FAILURE;
		}

		worlds[i].m_partList = new ModelDbPartList();

		for (j = 0; j < numParts; j++) {
			ModelDbPart* part = new ModelDbPart();

			if (part->Read(p_file) != SUCCESS) {
				return FAILURE;
			}

			worlds[i].m_partList->Append(part);
		}

		if (SDL_ReadIO(p_file, &worlds[i].m_numModels, sizeof(MxS32)) != sizeof(MxS32)) {
			return FAILURE;
		}

		worlds[i].m_models = new ModelDbModel[worlds[i].m_numModels];

		for (j = 0; j < worlds[i].m_numModels; j++) {
			if (worlds[i].m_models[j].Read(p_file) != SUCCESS) {
				return FAILURE;
			}
		}
	}

	p_worlds = worlds;
	p_numWorlds = numWorlds;
	return SUCCESS;
}

// FUNCTION: LEGO1 0x10028080
// FUNCTION: BETA10 0x100e6431
void FreeModelDbWorlds(ModelDbWorld*& p_worlds, MxS32 p_numWorlds)
{
	ModelDbWorld* worlds = p_worlds;

	for (MxS32 i = 0; i < p_numWorlds; i++) {
		delete[] worlds[i].m_worldName;

		ModelDbPartListCursor cursor(worlds[i].m_partList);
		ModelDbPart* part;

		while (cursor.Next(part)) {
			delete part;
		}

		delete worlds[i].m_partList;

		ModelDbModel* models = worlds[i].m_models;
		for (MxS32 j = 0; j < worlds[i].m_numModels; j++) {
			models[j].Free();
		}

		delete[] worlds[i].m_models;
	}

	delete[] p_worlds;
	p_worlds = NULL;
}
