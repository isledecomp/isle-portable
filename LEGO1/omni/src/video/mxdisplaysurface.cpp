#include "mxdisplaysurface.h"

#include "mxbitmap.h"
#include "mxdebug.h"
#include "mxmain.h"
#include "mxmisc.h"
#include "mxpalette.h"
#include "mxutilities.h"
#include "mxvideomanager.h"

#include <SDL3/SDL_log.h>
#include <assert.h>
#ifdef MINIWIN
#include "miniwin/windows.h"
#else
#include <windows.h>
#endif

DECOMP_SIZE_ASSERT(MxDisplaySurface, 0xac);

// GLOBAL: LEGO1 0x1010215c
MxU32 g_unk0x1010215c = 0;

// FUNCTION: LEGO1 0x100ba500
MxDisplaySurface::MxDisplaySurface()
{
	Init();
}

// FUNCTION: LEGO1 0x100ba5a0
MxDisplaySurface::~MxDisplaySurface()
{
	Destroy();
}

// FUNCTION: LEGO1 0x100ba610
void MxDisplaySurface::Init()
{
	m_ddSurface1 = NULL;
	m_ddSurface2 = NULL;
	m_ddClipper = NULL;
	m_16bitPal = NULL;
	m_32bitPal = NULL;
	m_initialized = FALSE;
	memset(&m_surfaceDesc, 0, sizeof(m_surfaceDesc));
}

// FUNCTION: LEGO1 0x100ba640
// FUNCTION: BETA10 0x1013f506
void MxDisplaySurface::ClearScreen()
{
	MxS32 i;
	MxS32 backBuffers;
	DDSURFACEDESC desc;

	if (m_videoParam.Flags().GetFlipSurfaces()) {
		backBuffers = m_videoParam.GetBackBuffers() + 1;
	}
	else {
		backBuffers = 2;
	}

	MxS32 width = m_videoParam.GetRect().GetWidth();
	MxS32 height = m_videoParam.GetRect().GetHeight();

	memset(&desc, 0, sizeof(desc));
	desc.dwSize = sizeof(desc);
	if (m_ddSurface2->GetSurfaceDesc(&desc) != DD_OK) {
		return;
	}

	DDBLTFX ddBltFx = {};
	ddBltFx.dwSize = sizeof(DDBLTFX);
	ddBltFx.dwFillColor = 0xFF000000;

	for (MxS32 i = 0; i < backBuffers; i++) {
		if (m_ddSurface2->Blt(NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &ddBltFx) == DDERR_SURFACELOST) {
			m_ddSurface2->Restore();
			m_ddSurface2->Blt(NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &ddBltFx);
		}

		if (m_videoParam.Flags().GetFlipSurfaces()) {
			m_ddSurface1->Flip(NULL, DDFLIP_WAIT);
		}
		else {
			DDBLTFX data;
			memset(&data, 0, sizeof(data));
			data.dwSize = sizeof(data);
			data.dwDDFX = DDBLTFX_NOTEARING;

			if (m_ddSurface1->Blt(NULL, m_ddSurface2, NULL, DDBLT_NONE, &data) == DDERR_SURFACELOST) {
				m_ddSurface1->Restore();
				m_ddSurface1->Blt(NULL, m_ddSurface2, NULL, DDBLT_NONE, &data);
			}
		}
	}
}

// FUNCTION: LEGO1 0x100ba750
// FUNCTION: BETA10 0x1013f6df
MxU8 MxDisplaySurface::CountTotalBitsSetTo1(MxU32 p_param)
{
	MxU8 count = 0;

	for (; p_param; p_param >>= 1) {
		count += ((MxU8) p_param & 1);
	}

	return count;
}

// FUNCTION: LEGO1 0x100ba770
// FUNCTION: BETA10 0x1013f724
MxU8 MxDisplaySurface::CountContiguousBitsSetTo1(MxU32 p_param)
{
	MxU8 count = 0;

	for (; (p_param & 1) == 0; p_param >>= 1) {
		count++;
	}

	return count;
}

// FUNCTION: LEGO1 0x100ba790
MxResult MxDisplaySurface::Init(
	MxVideoParam& p_videoParam,
	LPDIRECTDRAWSURFACE p_ddSurface1,
	LPDIRECTDRAWSURFACE p_ddSurface2,
	LPDIRECTDRAWCLIPPER p_ddClipper
)
{
	MxResult result = SUCCESS;

	m_videoParam = p_videoParam;
	m_ddSurface1 = p_ddSurface1;
	m_ddSurface2 = p_ddSurface2;
	m_ddClipper = p_ddClipper;
	m_initialized = FALSE;

	memset(&m_surfaceDesc, 0, sizeof(m_surfaceDesc));
	m_surfaceDesc.dwSize = sizeof(m_surfaceDesc);

	if (m_ddSurface2->GetSurfaceDesc(&m_surfaceDesc)) {
		result = FAILURE;
	}

	return result;
}

// FUNCTION: LEGO1 0x100ba7f0
MxResult MxDisplaySurface::Create(MxVideoParam& p_videoParam)
{
	DDSURFACEDESC ddsd;
	MxResult result = FAILURE;
	LPDIRECTDRAW lpDirectDraw = MVideoManager()->GetDirectDraw();
	HWND hWnd = MxOmni::GetInstance()->GetWindowHandle();

	m_initialized = TRUE;
	m_videoParam = p_videoParam;

	if (!m_videoParam.Flags().GetFullScreen()) {
		m_videoParam.Flags().SetFlipSurfaces(FALSE);
	}

	if (!m_videoParam.Flags().GetFlipSurfaces()) {
		m_videoParam.SetBackBuffers(1);
	}
	else {
		MxU32 backBuffers = m_videoParam.GetBackBuffers();

		if (backBuffers < 1) {
			m_videoParam.SetBackBuffers(1);
		}
		else if (backBuffers > 2) {
			m_videoParam.SetBackBuffers(2);
		}

		m_videoParam.Flags().SetBackBuffers(TRUE);
	}

	if (m_videoParam.Flags().GetFullScreen()) {
		MxS32 width = m_videoParam.GetRect().GetWidth();
		MxS32 height = m_videoParam.GetRect().GetHeight();

		if (lpDirectDraw->SetCooperativeLevel(hWnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN)) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DirectDraw::SetCooperativeLevel failed");
			goto done;
		}

		memset(&ddsd, 0, sizeof(ddsd));
		ddsd.dwSize = sizeof(ddsd);

		if (lpDirectDraw->GetDisplayMode(&ddsd)) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DirectDraw::GetDisplayMode failed");
			goto done;
		}

		MxS32 bitdepth = !m_videoParam.Flags().Get16Bit() ? 8 : ddsd.ddpfPixelFormat.dwRGBBitCount;

		if (ddsd.dwWidth != width || ddsd.dwHeight != height || ddsd.ddpfPixelFormat.dwRGBBitCount != bitdepth) {
			if (lpDirectDraw->SetDisplayMode(width, height, bitdepth)) {
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DirectDraw::SetDisplayMode failed");
				goto done;
			}
		}
	}

	if (m_videoParam.Flags().GetFlipSurfaces()) {
		memset(&ddsd, 0, sizeof(ddsd));
		ddsd.dwSize = sizeof(ddsd);
		ddsd.dwBackBufferCount = m_videoParam.GetBackBuffers();
		ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
		ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_3DDEVICE | DDSCAPS_COMPLEX;

		if (lpDirectDraw->CreateSurface(&ddsd, &m_ddSurface1, NULL)) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DirectDraw::CreateSurface failed");
			goto done;
		}

		ddsd.ddsCaps.dwCaps = DDSCAPS_BACKBUFFER;

		if (m_ddSurface1->GetAttachedSurface(&ddsd.ddsCaps, &m_ddSurface2)) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DirectDrawSurface::GetAttachedSurface failed");
			goto done;
		}
	}
	else {
		memset(&ddsd, 0, sizeof(ddsd));
		ddsd.dwSize = sizeof(ddsd);
		ddsd.dwFlags = DDSD_CAPS;
		ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

		if (lpDirectDraw->CreateSurface(&ddsd, &m_ddSurface1, NULL)) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DirectDraw::CreateSurface failed");
			goto done;
		}

		memset(&ddsd, 0, sizeof(ddsd));
		ddsd.dwSize = sizeof(ddsd);
		ddsd.dwFlags = DDSD_HEIGHT | DDSD_WIDTH | DDSD_CAPS;
		ddsd.dwWidth = m_videoParam.GetRect().GetWidth();
		ddsd.dwHeight = m_videoParam.GetRect().GetHeight();
		ddsd.ddsCaps.dwCaps = DDSCAPS_VIDEOMEMORY | DDSCAPS_3DDEVICE | DDSCAPS_OFFSCREENPLAIN;

		if (!m_videoParam.Flags().GetBackBuffers()) {
			ddsd.ddsCaps.dwCaps = DDSCAPS_3DDEVICE | DDSCAPS_SYSTEMMEMORY | DDSCAPS_OFFSCREENPLAIN;
		}

		if (lpDirectDraw->CreateSurface(&ddsd, &m_ddSurface2, NULL)) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DirectDraw::CreateSurface failed");
			goto done;
		}
	}

	memset(&m_surfaceDesc, 0, sizeof(m_surfaceDesc));
	m_surfaceDesc.dwSize = sizeof(m_surfaceDesc);

	if (!m_ddSurface2->GetSurfaceDesc(&m_surfaceDesc)) {
		if (!lpDirectDraw->CreateClipper(0, &m_ddClipper, NULL) && !m_ddClipper->SetHWnd(0, hWnd) &&
			!m_ddSurface1->SetClipper(m_ddClipper)) {
			result = SUCCESS;
		}
		else {
			SDL_LogError(
				SDL_LOG_CATEGORY_APPLICATION,
				"DirectDraw::CreateClipper or DirectDrawSurface::SetClipper failed"
			);
		}
	}

done:
	return result;
}

// FUNCTION: LEGO1 0x100baa90
void MxDisplaySurface::Destroy()
{
	if (m_initialized) {
		if (m_ddSurface2) {
			m_ddSurface2->Release();
		}

		if (m_ddSurface1) {
			m_ddSurface1->Release();
		}

		if (m_ddClipper) {
			m_ddClipper->Release();
		}
	}

	if (m_16bitPal) {
		delete[] m_16bitPal;
	}

	if (m_32bitPal) {
		delete[] m_32bitPal;
	}

	Init();
}

// FUNCTION: LEGO1 0x100baae0
// FUNCTION: BETA10 0x1013fe15
void MxDisplaySurface::SetPalette(MxPalette* p_palette)
{
	if ((m_surfaceDesc.ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED8) == DDPF_PALETTEINDEXED8) {
		m_ddSurface1->SetPalette(p_palette->CreateNativePalette());
		m_ddSurface2->SetPalette(p_palette->CreateNativePalette());

		if (!m_videoParam.Flags().GetFullScreen()) {
			struct {
				WORD m_palVersion;
				WORD m_palNumEntries;
				PALETTEENTRY m_palPalEntry[256];
			} lpal = {0x300, 256};

			p_palette->GetEntries((LPPALETTEENTRY) lpal.m_palPalEntry);

			HPALETTE hpal = CreatePalette((LPLOGPALETTE) &lpal);
			HDC hdc = ::GetDC(0);
			SelectPalette(hdc, hpal, FALSE);
			RealizePalette(hdc);
			::ReleaseDC(NULL, hdc);
			DeleteObject(hpal);
		}
	}

	MxS32 bitCount = m_surfaceDesc.ddpfPixelFormat.dwRGBBitCount;
	if (bitCount == 8) {
		return;
	}

	if (bitCount == 16) {
		if (!m_16bitPal) {
			m_16bitPal = new MxU16[256];
		}
	}
	else {
		if (!m_32bitPal) {
			m_32bitPal = new MxU32[256];
		}
	}

	PALETTEENTRY palette[256];
	p_palette->GetEntries(palette);

	auto& fmt = m_surfaceDesc.ddpfPixelFormat;

	MxU8 cRed = CountContiguousBitsSetTo1(fmt.dwRBitMask);
	MxU8 tRed = CountTotalBitsSetTo1(fmt.dwRBitMask);
	MxU8 cGreen = CountContiguousBitsSetTo1(fmt.dwGBitMask);
	MxU8 tGreen = CountTotalBitsSetTo1(fmt.dwGBitMask);
	MxU8 cBlue = CountContiguousBitsSetTo1(fmt.dwBBitMask);
	MxU8 tBlue = CountTotalBitsSetTo1(fmt.dwBBitMask);

	MxU8 cAlpha = 0;
	MxU8 tAlpha = 0;
	if (bitCount == 32) {
		cAlpha = CountContiguousBitsSetTo1(fmt.dwRGBAlphaBitMask);
		tAlpha = CountTotalBitsSetTo1(fmt.dwRGBAlphaBitMask);
	}

	for (MxS32 i = 0; i < 256; i++) {
		MxU32 red = (palette[i].peRed >> (8 - tRed)) << cRed;
		MxU32 green = (palette[i].peGreen >> (8 - tGreen)) << cGreen;
		MxU32 blue = (palette[i].peBlue >> (8 - tBlue)) << cBlue;

		if (bitCount == 16) {
			m_16bitPal[i] = static_cast<MxU16>(red | green | blue);
		}
		else {
			MxU32 alpha = 0xFF;
			if (bitCount == 32 && tAlpha > 0) {
				alpha = (0xFF >> (8 - tAlpha)) << cAlpha;
			}
			m_32bitPal[i] = red | green | blue | alpha;
		}
	}
}

// FUNCTION: LEGO1 0x100bacc0
// FUNCTION: BETA10 0x1014012b
void MxDisplaySurface::VTable0x28(
	MxBitmap* p_bitmap,
	MxS32 p_left,
	MxS32 p_top,
	MxS32 p_right,
	MxS32 p_bottom,
	MxS32 p_width,
	MxS32 p_height
)
{
	if (!GetRectIntersection(
			p_bitmap->GetBmiWidth(),
			p_bitmap->GetBmiHeightAbs(),
			m_videoParam.GetRect().GetWidth(),
			m_videoParam.GetRect().GetHeight(),
			&p_left,
			&p_top,
			&p_right,
			&p_bottom,
			&p_width,
			&p_height
		)) {
		return;
	}
	DDSURFACEDESC ddsd;
	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_CAPS;
	ddsd.dwWidth = p_width;
	ddsd.dwHeight = p_height;
	ddsd.ddpfPixelFormat = m_surfaceDesc.ddpfPixelFormat;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;

	LPDIRECTDRAWSURFACE tempSurface = nullptr;
	LPDIRECTDRAW draw = MVideoManager()->GetDirectDraw();
	HRESULT hr = draw->CreateSurface(&ddsd, &tempSurface, nullptr);
	if (hr != DD_OK || !tempSurface) {
		return;
	}

	if (m_surfaceDesc.ddpfPixelFormat.dwRGBBitCount != 32) {
		DDCOLORKEY colorKey;
		if (m_surfaceDesc.ddpfPixelFormat.dwRGBBitCount == 8) {
			colorKey.dwColorSpaceLowValue = colorKey.dwColorSpaceHighValue = 0x10;
		}
		else {
			colorKey.dwColorSpaceLowValue = colorKey.dwColorSpaceHighValue = RGB555_CREATE(0x1f, 0, 0x1f);
		}
		tempSurface->SetColorKey(DDCKEY_SRCBLT, &colorKey);
	}

	DDSURFACEDESC tempDesc;
	memset(&tempDesc, 0, sizeof(tempDesc));
	tempDesc.dwSize = sizeof(tempDesc);

	hr = tempSurface->Lock(NULL, &tempDesc, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);
	if (hr == DDERR_SURFACELOST) {
		tempSurface->Restore();
		hr = tempSurface->Lock(NULL, &tempDesc, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);
	}

	if (hr != DD_OK) {
		tempSurface->Release();
		return;
	}

	MxU8* data = p_bitmap->GetStart(p_left, p_top);

	MxS32 bytesPerPixel = m_surfaceDesc.ddpfPixelFormat.dwRGBBitCount / 8;
	MxU8* surface = (MxU8*) tempDesc.lpSurface;

	MxLong stride = (bytesPerPixel == 1) ? GetAdjustedStride(p_bitmap) : -p_width + GetAdjustedStride(p_bitmap);
	MxLong length = tempDesc.lPitch - (p_width * bytesPerPixel);

	for (MxS32 i = 0; i < p_height; i++) {
		if (bytesPerPixel == 1) {
			memcpy(surface, data, p_width);
			surface += length + p_width;
		}
		else if (bytesPerPixel == 2) {
			for (MxS32 j = 0; j < p_width; j++) {
				*(MxU16*) surface = m_16bitPal[*data++];
				surface += bytesPerPixel;
			}

			surface += length;
		}
		else {
			for (MxS32 j = 0; j < p_width; j++) {
				*(MxU32*) surface = m_32bitPal[*data++];
				surface += bytesPerPixel;
			}
			surface += length;
		}

		data += stride;
	}

	tempSurface->Unlock(NULL);

	if (m_videoParam.Flags().GetDoubleScaling()) {
		RECT destRect = {p_right, p_bottom, p_right + p_width * 2, p_bottom + p_height * 2};
		m_ddSurface2->Blt(&destRect, tempSurface, NULL, DDBLT_WAIT | DDBLT_KEYSRC, NULL);
	}
	else {
		m_ddSurface2->BltFast(p_right, p_bottom, tempSurface, NULL, DDBLTFAST_WAIT | DDBLTFAST_SRCCOLORKEY);
	}

	tempSurface->Release();
}

// FUNCTION: LEGO1 0x100bb1d0
// FUNCTION: BETA10 0x1014088e
void MxDisplaySurface::VTable0x30(
	MxBitmap* p_bitmap,
	MxS32 p_left,
	MxS32 p_top,
	MxS32 p_right,
	MxS32 p_bottom,
	MxS32 p_width,
	MxS32 p_height,
	MxBool p_RLE
)
{
	if (!GetRectIntersection(
			p_bitmap->GetBmiWidth(),
			p_bitmap->GetBmiHeightAbs(),
			m_videoParam.GetRect().GetWidth(),
			m_videoParam.GetRect().GetHeight(),
			&p_left,
			&p_top,
			&p_right,
			&p_bottom,
			&p_width,
			&p_height
		)) {
		return;
	}
	DDSURFACEDESC ddsd;
	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_CAPS;
	ddsd.dwWidth = p_width;
	ddsd.dwHeight = p_height;
	ddsd.ddpfPixelFormat = m_surfaceDesc.ddpfPixelFormat;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;

	LPDIRECTDRAW draw = MVideoManager()->GetDirectDraw();
	LPDIRECTDRAWSURFACE tempSurface = nullptr;
	if (draw->CreateSurface(&ddsd, &tempSurface, nullptr) != DD_OK || !tempSurface) {
		return;
	}

	DDCOLORKEY colorKey;
	colorKey.dwColorSpaceLowValue = colorKey.dwColorSpaceHighValue = 0;
	tempSurface->SetColorKey(DDCKEY_SRCBLT, &colorKey);

	DDSURFACEDESC tempDesc;
	memset(&tempDesc, 0, sizeof(tempDesc));
	tempDesc.dwSize = sizeof(tempDesc);

	if (tempSurface->Lock(NULL, &tempDesc, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL) != DD_OK) {
		tempSurface->Release();
		return;
	}

	MxU8* data = p_bitmap->GetStart(p_left, p_top);

	MxS32 bytesPerPixel = m_surfaceDesc.ddpfPixelFormat.dwRGBBitCount / 8;
	MxU8* surface = (MxU8*) tempDesc.lpSurface;

	if (p_RLE) {
		MxS32 size = p_bitmap->GetBmiHeader()->biSizeImage;
		DrawTransparentRLE(data, surface, size, p_width, p_height, tempDesc.lPitch, bytesPerPixel * 8);
	}
	else {
		MxLong stride = -p_width + GetAdjustedStride(p_bitmap);
		MxLong length = -bytesPerPixel * p_width + tempDesc.lPitch;

		for (MxS32 i = 0; i < p_height; i++) {
			for (MxS32 j = 0; j < p_width; j++) {
				if (*data != 0) {
					switch (bytesPerPixel) {
					case 1:
						*surface = *data;
						break;
					case 2:
						*(MxU16*) surface = m_16bitPal[*data];
						break;
					default:
						*(MxU32*) surface = m_32bitPal[*data];
						break;
					}
				}
				data++;
				surface += bytesPerPixel;
			}
			data += stride;
			surface += length;
		}
	}

	tempSurface->Unlock(NULL);

	m_ddSurface2->BltFast(p_right, p_bottom, tempSurface, NULL, DDBLTFAST_WAIT | DDBLTFAST_SRCCOLORKEY);

	tempSurface->Release();
}

// FUNCTION: LEGO1 0x100bb500
// FUNCTION: BETA10 0x10140cd6
void MxDisplaySurface::DrawTransparentRLE(
	MxU8*& p_bitmapData,
	MxU8*& p_surfaceData,
	MxU32 p_bitmapSize,
	MxS32 p_width,
	MxS32 p_height,
	MxLong p_pitch,
	MxU8 p_bpp
)
{
	/* Assumes partial RLE for the bitmap: only the skipped pixels are compressed.
	The drawn pixels are uncompressed. The procedure is:
	1. Read 3 bytes from p_bitmapData. Skip this many pixels on the surface.
	2. Read 3 bytes from p_bitmapData. Draw this many pixels on the surface.
	3. Repeat until the end of p_bitmapData is reached. */

	MxU8* end = p_bitmapData + p_bitmapSize;
	MxU8* surfCopy = p_surfaceData; // unused?

	// The total number of pixels drawn or skipped
	MxU32 count = 0;

	// Used in both 8 and 16 bit branches
	MxU32 skipCount;
	MxU32 drawCount;
	MxU32 t;

	if (p_bpp == 16) {
		// DECOMP: why goto?
		goto sixteen_bit;
	}

	while (p_bitmapData < end) {
		skipCount = *p_bitmapData++;
		t = *p_bitmapData++;
		skipCount += t << 8;
		t = *p_bitmapData++;
		skipCount += t << 16;

		MxS32 rowRemainder = p_width - count % p_width;
		count += skipCount;

		if (skipCount >= rowRemainder) {
			p_surfaceData += rowRemainder; // skip the rest of this row
			skipCount -= rowRemainder;
			p_surfaceData += p_pitch - p_width;               // seek to start of next row
			p_surfaceData += p_pitch * (skipCount / p_width); // skip entire rows if any
		}

		// skip any pixels at the start of this row
		p_surfaceData += skipCount % p_width;
		if (p_bitmapData >= end) {
			break;
		}

		drawCount = *p_bitmapData++;
		t = *p_bitmapData++;
		drawCount += t << 8;
		t = *p_bitmapData++;
		drawCount += t << 16;

		rowRemainder = p_width - count % p_width;
		count += drawCount;

		if (drawCount >= rowRemainder) {
			memcpy(p_surfaceData, p_bitmapData, rowRemainder);
			p_surfaceData += rowRemainder;
			p_bitmapData += rowRemainder;

			drawCount -= rowRemainder;

			// seek to start of bitmap on this screen row
			p_surfaceData += p_pitch - p_width;
			MxS32 rows = drawCount / p_width;

			for (MxU32 i = 0; i < rows; i++) {
				memcpy(p_surfaceData, p_bitmapData, p_width);
				p_bitmapData += p_width;
				p_surfaceData += p_pitch;
			}
		}

		MxS32 tail = drawCount % p_width;
		memcpy(p_surfaceData, p_bitmapData, tail);
		p_surfaceData += tail;
		p_bitmapData += tail;
	}
	return;

sixteen_bit:
	while (p_bitmapData < end) {
		skipCount = *p_bitmapData++;
		t = *p_bitmapData++;
		skipCount += t << 8;
		t = *p_bitmapData++;
		skipCount += t << 16;

		MxS32 rowRemainder = p_width - count % p_width;
		count += skipCount;

		if (skipCount >= rowRemainder) {
			p_surfaceData += 2 * rowRemainder;
			skipCount -= rowRemainder;
			p_surfaceData += p_pitch - 2 * p_width;
			p_surfaceData += p_pitch * (skipCount / p_width);
		}

		p_surfaceData += 2 * (skipCount % p_width);
		if (p_bitmapData >= end) {
			break;
		}

		drawCount = *p_bitmapData++;
		t = *p_bitmapData++;
		drawCount += t << 8;
		t = *p_bitmapData++;
		drawCount += t << 16;

		rowRemainder = p_width - count % p_width;
		count += drawCount;

		if (drawCount >= rowRemainder) {
			// memcpy
			for (MxU32 j = 0; j < rowRemainder; j++) {
				*((MxU16*) p_surfaceData) = m_16bitPal[*p_bitmapData++];
				p_surfaceData += 2;
			}

			drawCount -= rowRemainder;

			p_surfaceData += p_pitch - 2 * p_width;
			MxS32 rows = drawCount / p_width;

			for (MxU32 i = 0; i < rows; i++) {
				// memcpy
				for (MxS32 j = 0; j < p_width; j++) {
					*((MxU16*) p_surfaceData) = m_16bitPal[*p_bitmapData++];
					p_surfaceData += 2;
				}

				p_surfaceData += p_pitch - 2 * p_width;
			}
		}

		MxS32 tail = drawCount % p_width;
		// memcpy
		for (MxS32 j = 0; j < tail; j++) {
			*((MxU16*) p_surfaceData) = m_16bitPal[*p_bitmapData++];
			p_surfaceData += 2;
		}
	}
}

// FUNCTION: LEGO1 0x100bba50
void MxDisplaySurface::Display(MxS32 p_left, MxS32 p_top, MxS32 p_left2, MxS32 p_top2, MxS32 p_width, MxS32 p_height)
{
	if (m_videoParam.Flags().GetEnabled()) {
		if (m_videoParam.Flags().GetFlipSurfaces()) {
			if (g_unk0x1010215c < 2) {
				g_unk0x1010215c++;

				DDBLTFX ddbltfx = {};
				ddbltfx.dwSize = sizeof(ddbltfx);
				ddbltfx.dwFillColor = 0xFF000000;

				if (m_ddSurface2->Blt(NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &ddbltfx) != DD_OK) {
					SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "MxDisplaySurface::Display error\n");
				}
			}
			m_ddSurface1->Flip(NULL, DDFLIP_WAIT);
		}
		else {
			MxPoint32 point(0, 0);
			ClientToScreen(MxOmni::GetInstance()->GetWindowHandle(), (LPPOINT) &point);

			p_left2 += m_videoParam.GetRect().GetLeft() + point.GetX();
			p_top2 += m_videoParam.GetRect().GetTop() + point.GetY();

			MxRect32 a(MxPoint32(p_left, p_top), MxSize32(p_width + 1, p_height + 1));
			MxRect32 b(MxPoint32(p_left2, p_top2), MxSize32(p_width + 1, p_height + 1));

			DDBLTFX data;
			memset(&data, 0, sizeof(data));
			data.dwSize = sizeof(data);
			data.dwDDFX = DDBLTFX_NOTEARING;

			if (m_ddSurface1->Blt((LPRECT) &b, m_ddSurface2, (LPRECT) &a, DDBLT_NONE, &data) == DDERR_SURFACELOST) {
				m_ddSurface1->Restore();
				m_ddSurface1->Blt((LPRECT) &b, m_ddSurface2, (LPRECT) &a, DDBLT_NONE, &data);
			}
		}
	}
}

// FUNCTION: LEGO1 0x100bbc10
void MxDisplaySurface::GetDC(HDC* p_hdc)
{
	if (m_ddSurface2 && !m_ddSurface2->GetDC(p_hdc)) {
		return;
	}

	*p_hdc = NULL;
}

// FUNCTION: LEGO1 0x100bbc40
void MxDisplaySurface::ReleaseDC(HDC p_hdc)
{
	if (m_ddSurface2 && p_hdc) {
		m_ddSurface2->ReleaseDC(p_hdc);
	}
}

// FUNCTION: LEGO1 0x100bbc60
// FUNCTION: BETA10 0x10141745
LPDIRECTDRAWSURFACE MxDisplaySurface::VTable0x44(
	MxBitmap* p_bitmap,
	undefined4* p_ret,
	undefined4 p_doNotWriteToSurface,
	undefined4 p_transparent
)
{
	LPDIRECTDRAWSURFACE surface = NULL;
	LPDIRECTDRAW draw = MVideoManager()->GetDirectDraw();
	MxVideoParamFlags& flags = MVideoManager()->GetVideoParam().Flags();

	DDSURFACEDESC ddsd;
	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);

	if (draw->GetDisplayMode(&ddsd)) {
		return NULL;
	}

	ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
	ddsd.dwWidth = p_bitmap->GetBmiWidth();
	ddsd.dwHeight = p_bitmap->GetBmiHeightAbs();
	ddsd.ddpfPixelFormat = m_surfaceDesc.ddpfPixelFormat;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
	*p_ret = 0;
	ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;

	if (draw->CreateSurface(&ddsd, &surface, NULL) != DD_OK) {
		if (*p_ret) {
			*p_ret = 0;

			// Try creating bitmap surface in vram if system ram ran out
			ddsd.ddsCaps.dwCaps &= ~DDSCAPS_VIDEOMEMORY;
			ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;

			if (draw->CreateSurface(&ddsd, &surface, NULL) != DD_OK) {
				surface = NULL;
			}
		}
		else {
			surface = NULL;
		}
	}

	if (surface) {
		memset(&ddsd, 0, sizeof(ddsd));
		ddsd.dwSize = sizeof(ddsd);

		if (surface->Lock(NULL, &ddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY, 0) != DD_OK) {
			surface->Release();
			surface = NULL;
		}
		else if (p_doNotWriteToSurface) {
			assert(0);
		}
		else {
			MxU8* bitmapSrcPtr = p_bitmap->GetStart(0, 0);
			MxLong widthNormal = p_bitmap->GetBmiWidth();
			MxLong heightAbs = p_bitmap->GetBmiHeightAbs();

			MxLong newPitch = ddsd.lPitch;
			MxS32 rowSeek = p_bitmap->AlignToFourByte(widthNormal);
			if (!p_bitmap->IsTopDown()) {
				rowSeek *= -1;
			}

			MxS32 bytesPerPixel = ddsd.ddpfPixelFormat.dwRGBBitCount / 8;

			if (bytesPerPixel != 1) {
				if ((bytesPerPixel == 2 && m_16bitPal == NULL) || (bytesPerPixel != 2 && m_32bitPal == NULL)) {
					goto error;
				}
			}

			MxU32 transparentColor;
			switch (bytesPerPixel) {
			case 1:
				transparentColor = 0;
				break;
			case 2:
				transparentColor = RGB555_CREATE(0x1f, 0, 0x1f);
				break;
			default:
				transparentColor = RGB8888_CREATE(0, 0, 0, 0);
				break;
			}

			MxU8* surfacePtr = (MxU8*) ddsd.lpSurface;
			MxS32 pixelSize = bytesPerPixel;
			MxS32 adjustedRowSeek = rowSeek - widthNormal;
			MxS32 adjustedPitch = newPitch - pixelSize * widthNormal;

			for (MxS32 y = 0; y < heightAbs; y++) {
				for (MxS32 x = 0; x < widthNormal; x++) {
					switch (bytesPerPixel) {
					case 1: {
						// For 8-bit, just copy the pixel directly
						*surfacePtr = *bitmapSrcPtr;
						break;
					}
					case 2: {
						MxU16* surfaceData16 = (MxU16*) surfacePtr;
						if (p_transparent && *bitmapSrcPtr == 0) {
							*surfaceData16 = transparentColor;
						}
						else {
							*surfaceData16 = m_16bitPal[*bitmapSrcPtr];
						}
						break;
					}
					default: {
						MxU32* surfaceData32 = (MxU32*) surfacePtr;
						if (p_transparent && *bitmapSrcPtr == 0) {
							*surfaceData32 = transparentColor;
						}
						else {
							*surfaceData32 = m_32bitPal[*bitmapSrcPtr];
						}
						break;
					}
					}
					bitmapSrcPtr++;
					surfacePtr += pixelSize;
				}
				bitmapSrcPtr += adjustedRowSeek;
				surfacePtr += adjustedPitch;
			}

			surface->Unlock(ddsd.lpSurface);

			if (p_transparent && surface && bytesPerPixel != 4) {
				DDCOLORKEY key;
				key.dwColorSpaceLowValue = key.dwColorSpaceHighValue = transparentColor;
				surface->SetColorKey(DDCKEY_SRCBLT, &key);
			}
		}
	}

	return surface;

error:
	if (surface) {
		surface->Release();
	}

	return NULL;
}

// FUNCTION: LEGO1 0x100bbfb0
LPDIRECTDRAWSURFACE MxDisplaySurface::CopySurface(LPDIRECTDRAWSURFACE p_src)
{
	LPDIRECTDRAWSURFACE newSurface = NULL;
	IDirectDraw* draw = MVideoManager()->GetDirectDraw();

	DDSURFACEDESC ddsd;
	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);

	p_src->GetSurfaceDesc(&ddsd);

	if (draw->CreateSurface(&ddsd, &newSurface, NULL) != DD_OK) {
		return NULL;
	}

	RECT rect = {0, 0, (LONG) ddsd.dwWidth, (LONG) ddsd.dwHeight};

	if (newSurface->BltFast(0, 0, p_src, &rect, DDBLTFAST_WAIT) != DD_OK) {
		newSurface->Release();
		return NULL;
	}

	return newSurface;
}

// FUNCTION: LEGO1 0x100bc200
void MxDisplaySurface::VTable0x24(
	LPDDSURFACEDESC p_desc,
	MxBitmap* p_bitmap,
	MxS32 p_left,
	MxS32 p_top,
	MxS32 p_right,
	MxS32 p_bottom,
	MxS32 p_width,
	MxS32 p_height
)
{
	// DECOMP: Almost an exact copy of VTable0x28, except that it uses the argument DDSURFACEDESC
	// instead of getting one from GetDisplayMode.
	if (!GetRectIntersection(
			p_bitmap->GetBmiWidth(),
			p_bitmap->GetBmiHeightAbs(),
			m_videoParam.GetRect().GetWidth(),
			m_videoParam.GetRect().GetHeight(),
			&p_left,
			&p_top,
			&p_right,
			&p_bottom,
			&p_width,
			&p_height
		)) {
		return;
	}

	MxU8* data = p_bitmap->GetStart(p_left, p_top);

	if (m_videoParam.Flags().GetDoubleScaling()) {
		p_bottom *= 2;
		p_right *= 2;

		MxS32 bytesPerPixel = m_surfaceDesc.ddpfPixelFormat.dwRGBBitCount / 8;
		MxU8* surface = (MxU8*) p_desc->lpSurface + bytesPerPixel * p_right + (p_bottom * p_desc->lPitch);

		MxLong srcSkip = GetAdjustedStride(p_bitmap) - p_width;
		MxLong destSkip = p_desc->lPitch - 2 * bytesPerPixel * p_width;
		MxS32 copyWidth = 2 * bytesPerPixel * p_width;

		while (p_height--) {
			MxU8* surfaceBefore = surface;

			for (MxS32 x = 0; x < p_width; x++) {
				if (bytesPerPixel == 1) {
					surface[0] = surface[1] = *data;
				}
				else if (bytesPerPixel == 2) {
					MxU16 val = m_16bitPal[*data];
					((MxU16*) surface)[0] = ((MxU16*) surface)[1] = val;
				}
				else {
					MxU32 val = m_32bitPal[*data];
					((MxU32*) surface)[0] = ((MxU32*) surface)[1] = val;
				}
				surface += bytesPerPixel * 2;
				data++;
			}

			data += srcSkip;
			surface += destSkip;

			memcpy(surface, surfaceBefore, copyWidth);
			surface += p_desc->lPitch;
		}
	}
	else {
		MxS32 bytesPerPixel = m_surfaceDesc.ddpfPixelFormat.dwRGBBitCount / 8;
		MxU8* surface = (MxU8*) p_desc->lpSurface + bytesPerPixel * p_right + (p_bottom * p_desc->lPitch);

		MxLong srcSkip = GetAdjustedStride(p_bitmap) - p_width;
		MxLong destSkip = p_desc->lPitch - bytesPerPixel * p_width;

		for (MxS32 y = 0; y < p_height; y++) {
			for (MxS32 x = 0; x < p_width; x++) {
				if (bytesPerPixel == 1) {
					*surface = *data++;
				}
				else if (bytesPerPixel == 2) {
					*(MxU16*) surface = m_16bitPal[*data++];
				}
				else {
					*(MxU32*) surface = m_32bitPal[*data++];
				}
				surface += bytesPerPixel;
			}
			data += srcSkip;
			surface += destSkip;
		}
	}
}

// FUNCTION: LEGO1 0x100bc630
void MxDisplaySurface::VTable0x2c(
	LPDDSURFACEDESC p_desc,
	MxBitmap* p_bitmap,
	MxS32 p_left,
	MxS32 p_top,
	MxS32 p_right,
	MxS32 p_bottom,
	MxS32 p_width,
	MxS32 p_height,
	MxBool p_RLE
)
{
	// DECOMP: Almost an exact copy of VTable0x28, except that it uses the argument DDSURFACEDESC
	// instead of getting one from GetDisplayMode.
	if (!GetRectIntersection(
			p_bitmap->GetBmiWidth(),
			p_bitmap->GetBmiHeightAbs(),
			m_videoParam.GetRect().GetWidth(),
			m_videoParam.GetRect().GetHeight(),
			&p_left,
			&p_top,
			&p_right,
			&p_bottom,
			&p_width,
			&p_height
		)) {
		return;
	}

	MxU8* src = p_bitmap->GetStart(p_left, p_top);

	MxS32 bytesPerPixel = m_surfaceDesc.ddpfPixelFormat.dwRGBBitCount / 8;
	MxLong destStride = p_desc->lPitch;
	MxU8* dest = (MxU8*) p_desc->lpSurface + bytesPerPixel * p_right + (p_bottom * destStride);

	if (p_RLE) {
		DrawTransparentRLE(
			src,
			dest,
			p_bitmap->GetBmiHeader()->biSizeImage,
			p_width,
			p_height,
			destStride,
			m_surfaceDesc.ddpfPixelFormat.dwRGBBitCount
		);
	}
	else {
		MxLong srcStride = GetAdjustedStride(p_bitmap);
		MxLong srcSkip = srcStride - p_width;
		MxLong destSkip = destStride - bytesPerPixel * p_width;
		for (MxS32 i = 0; i < p_height; i++, src += srcSkip, dest += destSkip) {
			for (MxS32 j = 0; j < p_width; j++, src++) {
				if (*src != 0) {
					switch (bytesPerPixel) {
					case 1:
						*dest = *src;
						break;
					case 2:
						*(MxU16*) dest = m_16bitPal[*src];
						break;
					default:
						*(MxU32*) dest = m_32bitPal[*src];
						break;
					}
				}
				dest += bytesPerPixel;
			}
		}
	}
}

// FUNCTION: LEGO1 0x100bc8b0
LPDIRECTDRAWSURFACE MxDisplaySurface::FUN_100bc8b0(MxS32 p_width, MxS32 p_height)
{
	LPDIRECTDRAWSURFACE surface = NULL;

	LPDIRECTDRAW ddraw = MVideoManager()->GetDirectDraw();
	MxVideoParam& unused = MVideoManager()->GetVideoParam();

	DDSURFACEDESC surfaceDesc;
	memset(&surfaceDesc, 0, sizeof(surfaceDesc));
	surfaceDesc.dwSize = sizeof(surfaceDesc);

	if (ddraw->GetDisplayMode(&surfaceDesc) != DD_OK) {
		return NULL;
	}

	if (surfaceDesc.ddpfPixelFormat.dwRGBBitCount == 8) {
		return NULL;
	}

	surfaceDesc.dwWidth = p_width;
	surfaceDesc.dwHeight = p_height;
	surfaceDesc.dwFlags = DDSD_PIXELFORMAT | DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS;
	surfaceDesc.ddsCaps.dwCaps = DDSCAPS_VIDEOMEMORY | DDSCAPS_OFFSCREENPLAIN;

	if (ddraw->CreateSurface(&surfaceDesc, &surface, NULL) != DD_OK) {
		surfaceDesc.ddsCaps.dwCaps &= ~DDSCAPS_VIDEOMEMORY;
		surfaceDesc.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
		if (ddraw->CreateSurface(&surfaceDesc, &surface, NULL) != DD_OK) {
			return NULL;
		}
	}

	return surface;
}

LPDIRECTDRAWSURFACE MxDisplaySurface::CreateCursorSurface(const CursorBitmap* p_cursorBitmap)
{
	LPDIRECTDRAWSURFACE newSurface = NULL;
	IDirectDraw* draw = MVideoManager()->GetDirectDraw();
	MVideoManager();

	DDSURFACEDESC ddsd;
	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);

	if (draw->GetDisplayMode(&ddsd) != DD_OK) {
		return NULL;
	}

	MxS32 bytesPerPixel = ddsd.ddpfPixelFormat.dwRGBBitCount / 8;
	MxBool isAlphaAvailable = ((ddsd.ddpfPixelFormat.dwFlags & DDPF_ALPHAPIXELS) == DDPF_ALPHAPIXELS) &&
							  (ddsd.ddpfPixelFormat.dwRGBAlphaBitMask != 0);

	ddsd.dwWidth = p_cursorBitmap->width;
	ddsd.dwHeight = p_cursorBitmap->height;
	ddsd.dwFlags = DDSD_PIXELFORMAT | DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_VIDEOMEMORY | DDSCAPS_OFFSCREENPLAIN;

	if (draw->CreateSurface(&ddsd, &newSurface, NULL) != DD_OK) {
		ddsd.ddsCaps.dwCaps &= ~DDSCAPS_VIDEOMEMORY;
		ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;

		if (draw->CreateSurface(&ddsd, &newSurface, NULL) != DD_OK) {
			goto done;
		}
	}

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);

	if (newSurface->Lock(NULL, &ddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL) != DD_OK) {
		goto done;
	}
	else {
		for (int y = 0; y < p_cursorBitmap->height; y++) {
			for (int x = 0; x < p_cursorBitmap->width; x++) {
				MxS32 bitIndex = y * p_cursorBitmap->width + x;
				MxS32 byteIndex = bitIndex / 8;
				MxS32 bitOffset = 7 - (bitIndex % 8);

				MxBool isOpaque = (p_cursorBitmap->mask[byteIndex] >> bitOffset) & 1;
				MxBool isBlack = (p_cursorBitmap->data[byteIndex] >> bitOffset) & 1;

				switch (bytesPerPixel) {
				case 1: {
					MxU8* surface = (MxU8*) ddsd.lpSurface;

					MxU8 pixel;
					if (!isOpaque) {
						pixel = 0x10;
					}
					else {
						pixel = isBlack ? 0 : 0xff;
					}
				}
				case 2: {
					MxU16* surface = (MxU16*) ddsd.lpSurface;

					MxU16 pixel;
					if (!isOpaque) {
						pixel = RGB555_CREATE(0x1f, 0, 0x1f);
					}
					else {
						pixel = isBlack ? RGB555_CREATE(0, 0, 0) : RGB555_CREATE(0x1f, 0x1f, 0x1f);
					}

					surface[x + y * p_cursorBitmap->width] = pixel;
					break;
				}
				default: {
					MxU32* surface = (MxU32*) ddsd.lpSurface;

					MxS32 pixel;
					if (!isOpaque) {
						if (isAlphaAvailable) {
							pixel = RGB8888_CREATE(0, 0, 0, 0);
						}
						else {
							pixel = RGB8888_CREATE(0xff, 0, 0xff, 0);
						} // Transparent pixel
					}
					else {
						pixel = isBlack ? RGB8888_CREATE(0, 0, 0, 0xff) : RGB8888_CREATE(0xff, 0xff, 0xff, 0xff);
					}

					surface[x + y * p_cursorBitmap->width] = pixel;
					break;
				}
				}
			}
		}

		newSurface->Unlock(ddsd.lpSurface);
		switch (bytesPerPixel) {
		case 1: {
			DDCOLORKEY colorkey;
			colorkey.dwColorSpaceHighValue = 0x10;
			colorkey.dwColorSpaceLowValue = 0x10;
			newSurface->SetColorKey(DDCKEY_SRCBLT, &colorkey);
			break;
		}
		case 2: {
			DDCOLORKEY colorkey;
			colorkey.dwColorSpaceHighValue = RGB555_CREATE(0x1f, 0, 0x1f);
			colorkey.dwColorSpaceLowValue = RGB555_CREATE(0x1f, 0, 0x1f);
			newSurface->SetColorKey(DDCKEY_SRCBLT, &colorkey);
			break;
		}
		default: {
			if (!isAlphaAvailable) {
				DDCOLORKEY colorkey;
				colorkey.dwColorSpaceHighValue = RGB8888_CREATE(0xff, 0, 0xff, 0);
				colorkey.dwColorSpaceLowValue = RGB8888_CREATE(0xff, 0, 0xff, 0);
				newSurface->SetColorKey(DDCKEY_SRCBLT, &colorkey);
			}
			break;
		}
		}

		return newSurface;
	}

done:
	if (newSurface) {
		newSurface->Release();
	}

	return NULL;
}
