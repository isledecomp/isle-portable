#ifndef MXSMACK_H
#define MXSMACK_H

#include "decomp.h"
#include "mxrectlist.h"
#include "mxtypes.h"

#include <smacker.h>

struct MxBITMAPINFO;

// SIZE 0x6b8
struct MxSmack {
	smk m_smk;

	static MxResult LoadHeader(MxU8* p_data, MxU32 p_length, MxSmack* p_mxSmack);
	static void Destroy(MxSmack* p_mxSmack);
	static MxResult LoadFrame(
		MxBITMAPINFO* p_bitmapInfo,
		MxU8* p_bitmapData,
		MxSmack* p_mxSmack,
		MxU8* p_chunkData,
		MxBool& p_paletteChanged,
		MxU32 p_currentFrame,
		MxRectList* p_list
	);
};

#endif // MXSMACK_H
