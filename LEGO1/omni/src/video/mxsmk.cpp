#include "mxsmk.h"

#include "mxbitmap.h"

#include <string.h>

DECOMP_SIZE_ASSERT(SmackTag, 0x390);
DECOMP_SIZE_ASSERT(MxSmk, 0x6b8);

// FUNCTION: LEGO1 0x100c5a90
// FUNCTION: BETA10 0x10151e70
MxResult MxSmk::LoadHeader(MxU8* p_data, MxU32 p_length, MxSmk* p_mxSmk)
{
	p_mxSmk->m_smk = smk_open_memory_stream(p_data, p_length);

	if (p_mxSmk->m_smk == NULL) {
		return FAILURE;
	}

	smk_enable_video(p_mxSmk->m_smk, TRUE);
	return SUCCESS;
}

// FUNCTION: LEGO1 0x100c5d40
// FUNCTION: BETA10 0x10152298
void MxSmk::Destroy(MxSmk* p_mxSmk)
{
	if (p_mxSmk->m_smk != NULL) {
		smk_close(p_mxSmk->m_smk);
	}
}

// FUNCTION: LEGO1 0x100c5db0
// FUNCTION: BETA10 0x10152391
MxResult MxSmk::LoadFrame(
	MxBITMAPINFO* p_bitmapInfo,
	MxU8* p_bitmapData,
	MxSmk* p_mxSmk,
	MxU8* p_chunkData,
	MxBool& p_paletteChanged,
	MxU32 p_currentFrame,
	MxRect32List* p_list
)
{
	p_bitmapInfo->m_bmiHeader.biHeight = -MxBitmap::HeightAbs(p_bitmapInfo->m_bmiHeader.biHeight);

	unsigned long w, h;
	smk_info_video(p_mxSmk->m_smk, &w, &h, NULL);
	smk_set_chunk(p_mxSmk->m_smk, p_currentFrame, p_chunkData);

	if (p_currentFrame == 0) {
		smk_first(p_mxSmk->m_smk);
	}
	else {
		smk_next(p_mxSmk->m_smk);
	}

	memcpy(p_bitmapData, smk_get_video(p_mxSmk->m_smk), w * h);

	unsigned char frameType;
	smk_info_all(p_mxSmk->m_smk, NULL, NULL, &frameType, NULL);
	p_paletteChanged = frameType & 1;

	if (p_paletteChanged) {
		const unsigned char* palette = smk_get_palette(p_mxSmk->m_smk);

		for (MxU32 i = 0; i < 256; i++) {
			p_bitmapInfo->m_bmiColors[i].rgbBlue = palette[i * 3 + 2];
			p_bitmapInfo->m_bmiColors[i].rgbGreen = palette[i * 3 + 1];
			p_bitmapInfo->m_bmiColors[i].rgbRed = palette[i * 3];
		}
	}

	// [library:libsmacker] Calculate changed rects
	MxRect32* newRect = new MxRect32(0, 0, w - 1, h - 1);
	p_list->Append(newRect);
	return SUCCESS;
}
