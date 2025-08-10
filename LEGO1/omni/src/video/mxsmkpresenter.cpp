#include "mxsmkpresenter.h"

#include "decomp.h"
#include "mxdsmediaaction.h"
#include "mxmisc.h"
#include "mxpalette.h"
#include "mxvideomanager.h"

#include <smacker.h>

DECOMP_SIZE_ASSERT(MxSmkPresenter, 0x720);

// FUNCTION: LEGO1 0x100b3650
MxSmkPresenter::MxSmkPresenter()
{
	Init();
}

// FUNCTION: LEGO1 0x100b3870
MxSmkPresenter::~MxSmkPresenter()
{
	Destroy(TRUE);
}

// FUNCTION: LEGO1 0x100b38d0
void MxSmkPresenter::Init()
{
	m_currentFrame = 0;
	memset(&m_mxSmk, 0, sizeof(m_mxSmk));
	SetUseSurface(FALSE);
	SetUseVideoMemory(FALSE);
}

// FUNCTION: LEGO1 0x100b3900
void MxSmkPresenter::Destroy(MxBool p_fromDestructor)
{
	ENTER(m_criticalSection);

	MxSmk::Destroy(&m_mxSmk);
	Init();

	m_criticalSection.Leave();

	if (!p_fromDestructor) {
		MxVideoPresenter::Destroy(FALSE);
	}
}

// FUNCTION: LEGO1 0x100b3940
void MxSmkPresenter::LoadHeader(MxStreamChunk* p_chunk)
{
	MxSmk::LoadHeader(p_chunk->GetData(), p_chunk->GetLength(), &m_mxSmk);
}

// FUNCTION: LEGO1 0x100b3960
void MxSmkPresenter::CreateBitmap()
{
	if (m_frameBitmap) {
		delete m_frameBitmap;
	}

	unsigned long w, h;
	smk_info_video(m_mxSmk.m_smk, &w, &h, NULL);

	m_frameBitmap = new MxBitmap;
	m_frameBitmap->SetSize(w, h, NULL, FALSE);
}

// FUNCTION: LEGO1 0x100b3a00
void MxSmkPresenter::LoadFrame(MxStreamChunk* p_chunk)
{
	MxBITMAPINFO* bitmapInfo = m_frameBitmap->GetBitmapInfo();
	MxU8* bitmapData = m_frameBitmap->GetImage();
	MxU8* chunkData = p_chunk->GetData();

	MxBool paletteChanged;
	m_currentFrame++;
	ResetCurrentFrameAtEnd();

	MxRect32List rects(TRUE);
	MxSmk::LoadFrame(bitmapInfo, bitmapData, &m_mxSmk, chunkData, paletteChanged, m_currentFrame - 1, &rects);

	if (((MxDSMediaAction*) m_action)->GetPaletteManagement() && paletteChanged) {
		RealizePalette();
	}

	MxRect32 invalidateRect;
	MxRect32ListCursor cursor(&rects);
	MxRect32* rect;

	while (cursor.Next(rect)) {
		invalidateRect = *rect;
		invalidateRect += GetLocation();
		MVideoManager()->InvalidateRect(invalidateRect);
	}
}

// FUNCTION: LEGO1 0x100b4260
void MxSmkPresenter::ResetCurrentFrameAtEnd()
{
	// [library:libsmacker] Figure out if this functionality is still required
}

// FUNCTION: LEGO1 0x100b42c0
void MxSmkPresenter::RealizePalette()
{
	MxPalette* palette = m_frameBitmap->CreatePalette();
	MVideoManager()->RealizePalette(palette);
	delete palette;
}

// FUNCTION: LEGO1 0x100b42f0
MxResult MxSmkPresenter::AddToManager()
{
	return MxVideoPresenter::AddToManager();
}

// FUNCTION: LEGO1 0x100b4300
void MxSmkPresenter::Destroy()
{
	Destroy(FALSE);
}
