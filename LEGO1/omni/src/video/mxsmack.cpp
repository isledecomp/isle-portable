#include "mxsmack.h"

#include "mxbitmap.h"

#include <string.h>

DECOMP_SIZE_ASSERT(SmackTag, 0x390);
DECOMP_SIZE_ASSERT(MxSmack, 0x6b8);

// FUNCTION: LEGO1 0x100c5a90
MxResult MxSmack::LoadHeader(MxU8* p_data, MxU32 p_length, MxSmack* p_mxSmack)
{
	p_mxSmack->m_smk = smk_open_memory_stream(p_data, p_length);

	if (p_mxSmack->m_smk == NULL) {
		return FAILURE;
	}

	smk_enable_video(p_mxSmack->m_smk, TRUE);
	return SUCCESS;
}

// FUNCTION: LEGO1 0x100c5d40
void MxSmack::Destroy(MxSmack* p_mxSmack)
{
	if (p_mxSmack->m_smk != NULL) {
		smk_close(p_mxSmack->m_smk);
	}
}

// This should be refactored to somewhere else
inline MxLong AbsFlipped(MxLong p_value)
{
	return p_value > 0 ? p_value : -p_value;
}

// FUNCTION: LEGO1 0x100c5db0
MxResult MxSmack::LoadFrame(
	MxBITMAPINFO* p_bitmapInfo,
	MxU8* p_bitmapData,
	MxSmack* p_mxSmack,
	MxU8* p_chunkData,
	MxBool& p_paletteChanged,
	MxU32 p_currentFrame,
	MxRectList* p_list
)
{
	p_bitmapInfo->m_bmiHeader.biHeight = -AbsFlipped(p_bitmapInfo->m_bmiHeader.biHeight);

	unsigned long w, h;
	smk_info_video(p_mxSmack->m_smk, &w, &h, NULL);
	smk_set_chunk(p_mxSmack->m_smk, p_currentFrame, p_chunkData);

	if (p_currentFrame == 0) {
		smk_first(p_mxSmack->m_smk);
	}
	else {
		smk_next(p_mxSmack->m_smk);
	}

	memcpy(p_bitmapData, smk_get_video(p_mxSmack->m_smk), w * h);

	unsigned char frameType;
	smk_info_all(p_mxSmack->m_smk, NULL, NULL, &frameType, NULL);
	p_paletteChanged = frameType & 1;

	if (p_paletteChanged) {
		const unsigned char* palette = smk_get_palette(p_mxSmack->m_smk);

		for (MxU32 i = 0; i < 256; i++) {
			p_bitmapInfo->m_bmiColors[i].rgbBlue = palette[i * 3 + 2];
			p_bitmapInfo->m_bmiColors[i].rgbGreen = palette[i * 3 + 1];
			p_bitmapInfo->m_bmiColors[i].rgbRed = palette[i * 3];
		}
	}

	// TODO isle24: Calculate changed rects
	MxRect32* newRect = new MxRect32(0, 0, w - 1, h - 1);
	p_list->Append(newRect);
	return SUCCESS;
}
