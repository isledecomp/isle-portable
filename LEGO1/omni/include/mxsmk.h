#ifndef MXSMK_H
#define MXSMK_H

#include "decomp.h"
#include "mxrectlist.h"
#include "mxtypes.h"

#include <smacker.h>

struct MxBITMAPINFO;

// SIZE 0x6b8
struct MxSmk {
	smk m_smk;

	static MxResult LoadHeader(MxU8* p_data, MxU32 p_length, MxSmk* p_mxSmk);
	static void Destroy(MxSmk* p_mxSmk);
	static MxResult LoadFrame(
		MxBITMAPINFO* p_bitmapInfo,
		MxU8* p_bitmapData,
		MxSmk* p_mxSmk,
		MxU8* p_chunkData,
		MxBool& p_paletteChanged,
		MxU32 p_currentFrame,
		MxRectList* p_list
	);
};

#endif // MXSMK_H
