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

#ifndef MINIWIN
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
#endif
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
#ifdef MINIWIN
	ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	ddsd.ddpfPixelFormat.dwFlags = DDPF_PALETTEINDEXED8 | DDPF_RGB;
	ddsd.ddpfPixelFormat.dwRGBBitCount = 8;
#else
	ddsd.ddpfPixelFormat = m_surfaceDesc.ddpfPixelFormat;
#endif
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;

	LPDIRECTDRAWSURFACE tempSurface = nullptr;
	LPDIRECTDRAW draw = MVideoManager()->GetDirectDraw();
	HRESULT hr = draw->CreateSurface(&ddsd, &tempSurface, nullptr);
	if (hr != DD_OK || !tempSurface) {
		return;
	}

#ifdef MINIWIN
	MxBITMAPINFO* bmi = p_bitmap->GetBitmapInfo();
	if (bmi) {
		PALETTEENTRY pe[256];
		for (int i = 1; i < 256; i++) {
			pe[i].peRed = bmi->m_bmiColors[i].rgbRed;
			pe[i].peGreen = bmi->m_bmiColors[i].rgbGreen;
			pe[i].peBlue = bmi->m_bmiColors[i].rgbBlue;
			pe[i].peFlags = PC_NONE;
		}

		LPDIRECTDRAWPALETTE palette = nullptr;
		if (draw->CreatePalette(DDPCAPS_8BIT | DDPCAPS_ALLOW256, pe, &palette, NULL) == DD_OK && palette) {
			tempSurface->SetPalette(palette);
			palette->Release();
		}
	}
#endif

	if (ddsd.ddpfPixelFormat.dwRGBBitCount != 32) {
		DDCOLORKEY colorKey;
		colorKey.dwColorSpaceLowValue = colorKey.dwColorSpaceHighValue = 0;
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

	MxS32 bytesPerPixel = tempDesc.ddpfPixelFormat.dwRGBBitCount / 8;
	MxU8* surface = (MxU8*) tempDesc.lpSurface;

	MxLong stride = -p_width + GetAdjustedStride(p_bitmap);
	MxLong length = tempDesc.lPitch - (p_width * bytesPerPixel);

	for (MxS32 i = 0; i < p_height; i++) {
		if (bytesPerPixel == 1) {
			memcpy(surface, data, p_width);
			surface += length + p_width;
			data += p_width + stride;
		}
		else {
			for (MxS32 j = 0; j < p_width; j++) {
				if (bytesPerPixel == 2) {
					*(MxU16*) surface = m_16bitPal[*data];
				}
				else if (*data != 0) {
					*(MxU32*) surface = m_32bitPal[*data];
				}
				data++;
				surface += bytesPerPixel;
			}
			data += stride;
			surface += length;
		}
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
#ifdef MINIWIN
	ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	ddsd.ddpfPixelFormat.dwFlags = DDPF_PALETTEINDEXED8 | DDPF_RGB;
	ddsd.ddpfPixelFormat.dwRGBBitCount = 8;
#else
	ddsd.ddpfPixelFormat = m_surfaceDesc.ddpfPixelFormat;
#endif
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
#ifdef MINIWIN
		MxBITMAPINFO* bmi = p_bitmap->GetBitmapInfo();
		if (bmi) {
			PALETTEENTRY pe[256];
			for (int i = 0; i < 256; i++) {
				pe[i].peRed = bmi->m_bmiColors[i].rgbRed;
				pe[i].peGreen = bmi->m_bmiColors[i].rgbGreen;
				pe[i].peBlue = bmi->m_bmiColors[i].rgbBlue;
				pe[i].peFlags = PC_NONE;
			}
			if (p_transparent) {
				pe[0].peRed = 0;
				pe[0].peGreen = 0;
				pe[0].peBlue = 0;
			}
			LPDIRECTDRAWPALETTE palette = nullptr;
			if (draw->CreatePalette(DDPCAPS_8BIT | DDPCAPS_ALLOW256, pe, &palette, NULL) == DD_OK && palette) {
				surface->SetPalette(palette);
				palette->Release();
			}
		}
#endif

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

#ifdef MINIWIN
	LPDIRECTDRAWPALETTE srcPalette = nullptr;
	if (p_src->GetPalette(&srcPalette) == DD_OK && srcPalette) {
		PALETTEENTRY pe[256];
		if (srcPalette->GetEntries(0, 0, 256, pe) == DD_OK) {
			LPDIRECTDRAWPALETTE newPalette = nullptr;
			if (draw->CreatePalette(DDPCAPS_8BIT | DDPCAPS_ALLOW256, pe, &newPalette, NULL) == DD_OK) {
				newSurface->SetPalette(newPalette);
				newPalette->Release();
			}
		}
		srcPalette->Release();
	}
#endif

	RECT rect = {0, 0, (LONG) ddsd.dwWidth, (LONG) ddsd.dwHeight};

	if (newSurface->BltFast(0, 0, p_src, &rect, DDBLTFAST_WAIT) != DD_OK) {
		newSurface->Release();
		return NULL;
	}

	return newSurface;
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

LPDIRECTDRAWSURFACE MxDisplaySurface::CreateCursorSurface(const CursorBitmap* p_cursorBitmap, MxPalette* p_palette)
{
	LPDIRECTDRAWSURFACE newSurface = NULL;
	IDirectDraw* draw = MVideoManager()->GetDirectDraw();

	DDSURFACEDESC ddsd;
	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);

	if (draw->GetDisplayMode(&ddsd) != DD_OK) {
		return NULL;
	}

	ddsd.dwWidth = p_cursorBitmap->width;
	ddsd.dwHeight = p_cursorBitmap->height;
	ddsd.dwFlags = DDSD_PIXELFORMAT | DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_VIDEOMEMORY | DDSCAPS_OFFSCREENPLAIN;
#ifdef MINIWIN
	ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	ddsd.ddpfPixelFormat.dwFlags = DDPF_PALETTEINDEXED8 | DDPF_RGB;
	ddsd.ddpfPixelFormat.dwRGBBitCount = 8;
#endif

	MxS32 bytesPerPixel = ddsd.ddpfPixelFormat.dwRGBBitCount / 8;
	MxBool isAlphaAvailable = ((ddsd.ddpfPixelFormat.dwFlags & DDPF_ALPHAPIXELS) == DDPF_ALPHAPIXELS) &&
							  (ddsd.ddpfPixelFormat.dwRGBAlphaBitMask != 0);

	if (draw->CreateSurface(&ddsd, &newSurface, NULL) != DD_OK) {
		ddsd.ddsCaps.dwCaps &= ~DDSCAPS_VIDEOMEMORY;
		ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;

		if (draw->CreateSurface(&ddsd, &newSurface, NULL) != DD_OK) {
			goto done;
		}
	}

#ifdef MINIWIN
	newSurface->SetPalette(p_palette->CreateNativePalette());
#endif

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
					surface[x + y * p_cursorBitmap->width] = pixel;
					break;
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
			colorkey.dwColorSpaceHighValue = colorkey.dwColorSpaceLowValue = 0x10;
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
