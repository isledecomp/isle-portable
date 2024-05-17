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
	memset(&m_mxSmack, 0, sizeof(m_mxSmack));
	SetBit1(FALSE);
	SetBit2(FALSE);
}

// FUNCTION: LEGO1 0x100b3900
void MxSmkPresenter::Destroy(MxBool p_fromDestructor)
{
	m_criticalSection.Enter();

	MxSmack::Destroy(&m_mxSmack);
	Init();

	m_criticalSection.Leave();

	if (!p_fromDestructor) {
		MxVideoPresenter::Destroy(FALSE);
	}
}

// FUNCTION: LEGO1 0x100b3940
void MxSmkPresenter::LoadHeader(MxStreamChunk* p_chunk)
{
	MxSmack::LoadHeader(p_chunk->GetData(), p_chunk->GetLength(), &m_mxSmack);
}

// FUNCTION: LEGO1 0x100b3960
void MxSmkPresenter::CreateBitmap()
{
	if (m_frameBitmap) {
		delete m_frameBitmap;
	}

	unsigned long w, h;
	smk_info_video(m_mxSmack.m_smk, &w, &h, NULL);

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
	VTable0x88();

	MxRectList list(TRUE);
	MxSmack::LoadFrame(bitmapInfo, bitmapData, &m_mxSmack, chunkData, paletteChanged, m_currentFrame - 1, &list);

	if (((MxDSMediaAction*) m_action)->GetPaletteManagement() && paletteChanged) {
		RealizePalette();
	}

	MxRect32 invalidateRect;
	MxRectListCursor cursor(&list);
	MxRect32* rect;

	while (cursor.Next(rect)) {
		invalidateRect = *rect;
		invalidateRect.AddPoint(GetLocation());
		MVideoManager()->InvalidateRect(invalidateRect);
	}
}

// FUNCTION: LEGO1 0x100b4260
void MxSmkPresenter::VTable0x88()
{
	// TODO isle24
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
