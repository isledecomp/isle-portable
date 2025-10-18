#include "legovideomanager.h"

#include "3dmanager/lego3dmanager.h"
#include "legoinputmanager.h"
#include "legomain.h"
#include "misc.h"
#include "mxdirectx/legodxinfo.h"
#include "mxdirectx/mxdirect3d.h"
#include "mxdirectx/mxstopwatch.h"
#include "mxdisplaysurface.h"
#include "mxgeometry/mxmatrix.h"
#include "mxmisc.h"
#include "mxpalette.h"
#include "mxregion.h"
#include "mxtimer.h"
#include "mxtransitionmanager.h"
#include "realtime/realtime.h"
#include "roi/legoroi.h"
#include "tgl/d3drm/impl.h"
#include "viewmanager/viewroi.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>
#include <stdio.h>

DECOMP_SIZE_ASSERT(LegoVideoManager, 0x590)
DECOMP_SIZE_ASSERT(MxStopWatch, 0x18)
DECOMP_SIZE_ASSERT(MxFrequencyMeter, 0x20)

// FUNCTION: LEGO1 0x1007aa20
// FUNCTION: BETA10 0x100d5a00
LegoVideoManager::LegoVideoManager()
{
	m_renderer = NULL;
	m_3dManager = NULL;
	m_viewROI = NULL;
	m_direct3d = NULL;
	m_unk0xe6 = FALSE;
	memset(m_unk0x78, 0, sizeof(m_unk0x78));
	m_unk0x78[0] = 0x6c;
	m_phonemeRefList = NULL;
	m_isFullscreenMovie = FALSE;
	m_palette = NULL;
	m_stopWatch = NULL;
	m_drawCursor = FALSE;
	m_cursorX = m_cursorY;
	m_cursorYCopy = m_cursorY;
	m_cursorXCopy = m_cursorY;
	m_cursorSurface = NULL;
	m_fullScreenMovie = FALSE;
	m_drawFPS = FALSE;
	m_unk0x528 = NULL;
	m_arialFont = NULL;
	m_unk0xe5 = FALSE;
	m_unk0x554 = FALSE;
	m_paused = FALSE;
}

// FUNCTION: LEGO1 0x1007ab40
LegoVideoManager::~LegoVideoManager()
{
	Destroy();
	delete m_palette;
}

// FUNCTION: LEGO1 0x1007abb0
MxResult LegoVideoManager::CreateDirect3D()
{
	if (!m_direct3d) {
		m_direct3d = new MxDirect3D;
	}

	return m_direct3d ? SUCCESS : FAILURE;
}

// FUNCTION: LEGO1 0x1007ac40
// FUNCTION: BETA10 0x100d5cf4
MxResult LegoVideoManager::Create(MxVideoParam& p_videoParam, MxU32 p_frequencyMS, MxBool p_createThread)
{
	MxResult result = FAILURE;
	MxBool paletteCreated = FALSE;
	MxS32 deviceNum = -1;
	Direct3DDeviceInfo* device = NULL;
	MxDriver* driver = NULL;
	LegoDeviceEnumerate deviceEnumerate;
	Mx3DPointFloat posVec(0.0, 1.25, -50.0);
	Mx3DPointFloat dirVec(0.0, 0.0, 1.0);
	Mx3DPointFloat upVec(0.0, 1.0, 0.0);
	MxMatrix outMatrix;
	HWND hwnd = MxOmni::GetInstance()->GetWindowHandle();
	MxS32 bits = p_videoParam.Flags().Get16Bit() ? 16 : 8;

	if (!p_videoParam.GetPalette()) {
		MxPalette* palette = new MxPalette;
		p_videoParam.SetPalette(palette);

		if (!p_videoParam.GetPalette()) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "MxPalette::GetPalette returned NULL palette");
			goto done;
		}
		paletteCreated = TRUE;
	}

	PALETTEENTRY paletteEntries[256];
	p_videoParam.GetPalette()->GetEntries(paletteEntries);

#ifndef __WIIU__
	if (CreateDirect3D() != SUCCESS) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "::CreateDirect3D failed");
		goto done;
	}

	if (deviceEnumerate.DoEnumerate(hwnd) != SUCCESS) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "LegoDeviceEnumerate::DoEnumerate failed");
		goto done;
	}

	if (p_videoParam.GetDeviceName()) {
		deviceNum = deviceEnumerate.ParseDeviceName(p_videoParam.GetDeviceName());
		if (deviceNum >= 0) {
			if ((deviceNum = deviceEnumerate.GetDevice(deviceNum, driver, device)) != SUCCESS) {
				deviceNum = -1;
			}
		}
	}

	if (deviceNum < 0) {
		deviceEnumerate.FUN_1009d210();
		deviceNum = deviceEnumerate.GetBestDevice();
		deviceNum = deviceEnumerate.GetDevice(deviceNum, driver, device);
	}

	m_direct3d->SetDevice(deviceEnumerate, driver, device);
#endif

	if (driver->m_ddCaps.dwCaps2 != DDCAPS2_CERTIFIED && driver->m_ddCaps.dwSVBRops[7] != 2) {
		p_videoParam.Flags().SetLacksLightSupport(TRUE);
	}
	else {
		p_videoParam.Flags().SetLacksLightSupport(FALSE);
	}

	ViewROI::SetLightSupport(p_videoParam.Flags().GetLacksLightSupport() == FALSE);

	if (!m_direct3d->Create(
			hwnd,
			p_videoParam.Flags().GetFullScreen(),
			p_videoParam.Flags().GetFlipSurfaces(),
			p_videoParam.Flags().GetBackBuffers() == FALSE,
			p_videoParam.GetRect().GetWidth(),
			p_videoParam.GetRect().GetHeight(),
			bits,
			paletteEntries,
			sizeof(paletteEntries) / sizeof(paletteEntries[0])
		)) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "MxDirect3D::Create failed");
		goto done;
	}

	if (MxVideoManager::VTable0x28(
			p_videoParam,
			m_direct3d->DirectDraw(),
			m_direct3d->Direct3D(),
			m_direct3d->FrontBuffer(),
			m_direct3d->BackBuffer(),
			m_direct3d->Clipper(),
			p_frequencyMS,
			p_createThread
		) != SUCCESS) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "MxVideoManager::VTable0x28 failed");
		goto done;
	}

	m_renderer = Tgl::CreateRenderer();

	if (!m_renderer) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Tgl::CreateRenderer failed");
		goto done;
	}

	m_3dManager = new Lego3DManager;

	if (!m_3dManager) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Lego3DManager::Lego3DManager failed");
		goto done;
	}

	Lego3DManager::CreateStruct createStruct;
	memset(&createStruct, 0, sizeof(createStruct));
	createStruct.m_hWnd = LegoOmni::GetInstance()->GetWindowHandle();
	createStruct.m_pDirectDraw = m_pDirectDraw;
	createStruct.m_pFrontBuffer = m_displaySurface->GetDirectDrawSurface1();
	createStruct.m_pBackBuffer = m_displaySurface->GetDirectDrawSurface2();
	createStruct.m_pPalette = m_videoParam.GetPalette()->CreateNativePalette();
	createStruct.m_isFullScreen = FALSE;
	createStruct.m_isWideViewAngle = m_videoParam.Flags().GetWideViewAngle();
	createStruct.m_direct3d = m_direct3d->Direct3D();
	createStruct.m_d3dDevice = m_direct3d->Direct3DDevice();

	if (!m_3dManager->Create(createStruct)) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Lego3DManager::Create failed");
		goto done;
	}

	ViewLODList* pLODList;

	if (ConfigureD3DRM() != SUCCESS) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "LegoVideoManager::ConfigureD3DRM failed");
		goto done;
	}

	pLODList = m_3dManager->GetViewLODListManager()->Create("CameraROI", 1);
	m_viewROI = new TimeROI(m_renderer, pLODList, Timer()->GetTime());
	pLODList->Release();

	CalcLocalTransform(posVec, dirVec, upVec, outMatrix);
	m_viewROI->WrappedSetLocal2WorldWithWorldDataUpdate(outMatrix);

	m_3dManager->Add(*m_viewROI);
	m_3dManager->SetPointOfView(*m_viewROI);

	m_phonemeRefList = new LegoPhonemeList;
	SetRender3D(FALSE);
	m_stopWatch = new MxStopWatch;
	m_stopWatch->Start();

	result = SUCCESS;

done:
	if (paletteCreated) {
		delete p_videoParam.GetPalette();
		p_videoParam.SetPalette(NULL);
	}

	return result;
}

// FUNCTION: LEGO1 0x1007b5e0
// FUNCTION: BETA10 0x100d6816
void LegoVideoManager::Destroy()
{
	if (m_cursorSurface != NULL) {
		m_cursorSurface->Release();
		m_cursorSurface = NULL;
	}

	if (m_unk0x528 != NULL) {
		m_unk0x528->Release();
		m_unk0x528 = NULL;
	}

	if (m_arialFont != NULL) {
		DeleteObject(m_arialFont);
		m_arialFont = NULL;
	}

	delete m_renderer;

	if (m_viewROI != NULL) {
		if (m_3dManager != NULL) {
			m_3dManager->Remove(*m_viewROI);
		}

		delete m_viewROI;
	}

	delete m_3dManager;
	MxVideoManager::Destroy();
	delete m_phonemeRefList;
	delete m_stopWatch;
}

// FUNCTION: LEGO1 0x1007b6a0
void LegoVideoManager::MoveCursor(MxS32 p_cursorX, MxS32 p_cursorY)
{
	m_cursorX = p_cursorX;
	m_cursorY = p_cursorY;

	if (640 < p_cursorX) {
		m_cursorX = 640;
	}

	if (480 < p_cursorY) {
		m_cursorY = 480;
	}
}

// FUNCTION: LEGO1 0x1007b6f0
void LegoVideoManager::ToggleFPS(MxBool p_visible)
{
	if (p_visible && !m_drawFPS) {
		m_drawFPS = TRUE;
		m_unk0x550 = 1.0;
		m_unk0x54c = Timer()->GetTime();
	}
	else {
		m_drawFPS = p_visible;
	}
}

// FUNCTION: LEGO1 0x1007b770
// STUB: BETA10 0x100d69cc
MxResult LegoVideoManager::Tickle()
{
#ifndef BETA10
	if (m_unk0x554 && !m_videoParam.Flags().GetFlipSurfaces() &&
		TransitionManager()->GetTransitionType() == MxTransitionManager::e_idle) {
		Sleep(30);
	}
#endif

	m_stopWatch->Stop();
	m_elapsedSeconds = m_stopWatch->ElapsedSeconds();
	m_stopWatch->Reset();
	m_stopWatch->Start();

#ifndef __WIIU__
	m_direct3d->RestoreSurfaces();
#endif

	SortPresenterList();

	MxPresenter* presenter;
	MxPresenterListCursor cursor(m_presenters);

	while (cursor.Next(presenter)) {
		presenter->Tickle();
	}

	if (m_render3d && !m_paused) {
		m_3dManager->GetLego3DView()->GetView()->Clear();
	}

	MxRect32 rect(0, 0, m_videoParam.GetRect().GetWidth() - 1, m_videoParam.GetRect().GetHeight() - 1);
	InvalidateRect(rect);

	if (!m_paused && (m_render3d || m_unk0xe5)) {
		cursor.Reset();

		while (cursor.Next(presenter) && presenter->GetDisplayZ() >= 0) {
			presenter->PutData();
		}

		if (!m_unk0xe5) {
			m_3dManager->Render(0.0);
			m_3dManager->GetLego3DView()->GetDevice()->Update();
		}

		cursor.Prev();

		while (cursor.Next(presenter)) {
			presenter->PutData();
		}

		if (m_drawCursor) {
			DrawCursor();
		}

		if (m_drawFPS) {
			DrawFPS();
		}
	}
	else if (m_fullScreenMovie) {
		MxPresenter* presenter;
		MxPresenterListCursor cursor(m_presenters);

		if (cursor.Last(presenter)) {
			presenter->PutData();
		}
	}

	if (!m_paused) {
		if (m_render3d && m_videoParam.Flags().GetFlipSurfaces()) {
			m_3dManager->GetLego3DView()
				->GetView()
				->ForceUpdate(0, 0, m_videoParam.GetRect().GetWidth(), m_videoParam.GetRect().GetHeight());
		}

		UpdateRegion();
	}

	m_region->Reset();
	return SUCCESS;
}

inline void LegoVideoManager::DrawCursor()
{
	if (m_cursorX != m_cursorXCopy || m_cursorY != m_cursorYCopy) {
		if (m_cursorX >= 0 && m_cursorY >= 0) {
			m_cursorXCopy = m_cursorX;
			m_cursorYCopy = m_cursorY;
		}
	}

	LPDIRECTDRAWSURFACE ddSurface2 = m_displaySurface->GetDirectDrawSurface2();

	if (!m_cursorSurface) {
		return;
	}

	ddSurface2
		->BltFast(m_cursorXCopy, m_cursorYCopy, m_cursorSurface, &m_cursorRect, DDBLTFAST_WAIT | DDBLTFAST_SRCCOLORKEY);
}

// FUNCTION: LEGO1 0x1007bbc0
void LegoVideoManager::DrawFPS()
{
	if (m_unk0x528 == NULL) {
		int width = 64; // Big enough for 9999.99
		int height = 16;

		m_unk0x528 = m_displaySurface->FUN_100bc8b0(width, height);
		SetRect(&m_fpsRect, 0, 0, width, height);

		if (m_unk0x528 == NULL) {
			return;
		}

		DDCOLORKEY colorKey;
		memset(&colorKey, 0, sizeof(colorKey));
		m_unk0x528->SetColorKey(DDCKEY_SRCBLT, &colorKey);

		DDSURFACEDESC surfaceDesc;
		memset(&surfaceDesc, 0, sizeof(surfaceDesc));
		surfaceDesc.dwSize = sizeof(surfaceDesc);

		if (m_unk0x528->Lock(NULL, &surfaceDesc, DDLOCK_WAIT, NULL) != DD_OK) {
			m_unk0x528->Release();
			m_unk0x528 = NULL;
		}
		else {
			DWORD i;
			char* ptr = (char*) surfaceDesc.lpSurface;

			for (i = 0; i < surfaceDesc.dwHeight; i++) {
				memset(ptr, 0, surfaceDesc.lPitch);
				ptr += surfaceDesc.lPitch;
			}

			m_unk0x528->Unlock(surfaceDesc.lpSurface);
			m_unk0x54c = Timer()->GetTime();
			m_unk0x550 = 1.f;
		}
	}
	else {
		if (Timer()->GetTime() > m_unk0x54c + 5000.f) {
			char buffer[32];
			MxFloat time = (Timer()->GetTime() - m_unk0x54c) / 1000.0f;
			sprintf(buffer, "%.02f", m_unk0x550 / time);
			m_unk0x54c = Timer()->GetTime();

			DDSURFACEDESC surfaceDesc;
			memset(&surfaceDesc, 0, sizeof(surfaceDesc));
			surfaceDesc.dwSize = sizeof(surfaceDesc);

			if (m_unk0x528->Lock(NULL, &surfaceDesc, DDLOCK_WAIT, NULL) == DD_OK) {
				memset(surfaceDesc.lpSurface, 0, surfaceDesc.lPitch * surfaceDesc.dwHeight);

				DrawTextToSurface32((uint8_t*) surfaceDesc.lpSurface, surfaceDesc.lPitch, 0, 0, buffer, 0xFF0000FF);

				m_unk0x528->Unlock(surfaceDesc.lpSurface);
				m_unk0x550 = 1.f;
			}
		}
		else {
			m_unk0x550 += 1.f;
		}

		if (m_unk0x528 != NULL) {
			m_displaySurface->GetDirectDrawSurface2()
				->BltFast(20, 20, m_unk0x528, &m_fpsRect, DDBLTFAST_WAIT | DDBLTFAST_SRCCOLORKEY);
			m_3dManager->GetLego3DView()->GetView()->ForceUpdate(20, 20, m_fpsRect.right, m_fpsRect.bottom);
		}
	}
}

// FUNCTION: LEGO1 0x1007c080
// FUNCTION: BETA10 0x100d6d28
MxPresenter* LegoVideoManager::GetPresenterAt(MxS32 p_x, MxS32 p_y)
{
	MxPresenterListCursor cursor(m_presenters);
	MxPresenter* presenter;

	while (cursor.Prev(presenter)) {
		if (presenter->IsHit(p_x, p_y)) {
			return presenter;
		}
	}

	return NULL;
}

// FUNCTION: LEGO1 0x1007c180
// FUNCTION: BETA10 0x100d6df4
MxPresenter* LegoVideoManager::GetPresenterByActionObjectName(const char* p_actionObjectName)
{
	MxPresenterListCursor cursor(m_presenters);
	MxPresenter* presenter;

	while (TRUE) {
		if (!cursor.Prev(presenter)) {
			return NULL;
		}

		if (!presenter->GetAction()) {
			continue;
		}

		if (SDL_strcasecmp(presenter->GetAction()->GetObjectName(), p_actionObjectName) == 0) {
			return presenter;
		}
	}
}

// FUNCTION: LEGO1 0x1007c290
// FUNCTION: BETA10 0x100d731e
MxResult LegoVideoManager::RealizePalette(MxPalette* p_pallete)
{
	if (p_pallete && m_videoParam.GetPalette()) {
		p_pallete->GetEntries(m_paletteEntries);
		m_videoParam.GetPalette()->SetEntries(m_paletteEntries);
		m_displaySurface->SetPalette(m_videoParam.GetPalette());
	}

	return SUCCESS;
}

// FUNCTION: LEGO1 0x1007c2d0
MxResult LegoVideoManager::ResetPalette(MxBool p_ignoreSkyColor)
{
	MxResult result = FAILURE;

	if (m_videoParam.GetPalette() != NULL) {
		m_videoParam.GetPalette()->Reset(p_ignoreSkyColor);
		m_displaySurface->SetPalette(m_videoParam.GetPalette());
		result = SUCCESS;
	}

	return result;
}

// FUNCTION: LEGO1 0x1007c300
void LegoVideoManager::EnableFullScreenMovie(MxBool p_enable)
{
	EnableFullScreenMovie(p_enable, TRUE);
}

// FUNCTION: LEGO1 0x1007c310
void LegoVideoManager::EnableFullScreenMovie(MxBool p_enable, MxBool p_scale)
{
	if (m_isFullscreenMovie != p_enable) {
		m_isFullscreenMovie = p_enable;

		if (p_enable) {
			m_palette = m_videoParam.GetPalette()->Clone();
			OverrideSkyColor(FALSE);

			m_displaySurface->GetVideoParam().Flags().SetDoubleScaling(p_scale);

			m_render3d = FALSE;
			m_fullScreenMovie = TRUE;
		}
		else {
			m_displaySurface->ClearScreen();
			m_displaySurface->GetVideoParam().Flags().SetDoubleScaling(FALSE);

			// restore previous pallete
			RealizePalette(m_palette);
			delete m_palette;
			m_palette = NULL;

			// update region where video used to be
			MxRect32 rect(
				0,
				0,
				m_videoParam.GetRect().GetRight() - m_videoParam.GetRect().GetLeft(),
				m_videoParam.GetRect().GetBottom() - m_videoParam.GetRect().GetTop()
			);

			InvalidateRect(rect);
			UpdateRegion();
			OverrideSkyColor(TRUE);

			m_render3d = TRUE;
			m_fullScreenMovie = FALSE;
		}
	}

	if (p_enable) {
		m_displaySurface->GetVideoParam().Flags().SetDoubleScaling(p_scale);
	}
	else {
		m_displaySurface->GetVideoParam().Flags().SetDoubleScaling(FALSE);
	}
}

// FUNCTION: LEGO1 0x1007c440
void LegoVideoManager::SetSkyColor(float p_red, float p_green, float p_blue)
{
	PALETTEENTRY colorStructure;

	colorStructure.peRed = (p_red * 255.0f);
	colorStructure.peGreen = (p_green * 255.0f);
	colorStructure.peBlue = (p_blue * 255.0f);
	colorStructure.peFlags = D3DPAL_RESERVED | PC_NOCOLLAPSE;
	m_videoParam.GetPalette()->SetSkyColor(&colorStructure);
	m_videoParam.GetPalette()->SetOverrideSkyColor(TRUE);
	m_3dManager->GetLego3DView()->GetView()->SetBackgroundColor(p_red, p_green, p_blue);
}

// FUNCTION: LEGO1 0x1007c4c0
void LegoVideoManager::OverrideSkyColor(MxBool p_shouldOverride)
{
	m_videoParam.GetPalette()->SetOverrideSkyColor(p_shouldOverride);
}

// FUNCTION: LEGO1 0x1007c4d0
// FUNCTION: BETA10 0x100d77d3
void LegoVideoManager::UpdateView(MxU32 p_x, MxU32 p_y, MxU32 p_width, MxU32 p_height)
{
	if (p_width == 0) {
		p_width = m_videoParam.GetRect().GetWidth();
	}
	if (p_height == 0) {
		p_height = m_videoParam.GetRect().GetHeight();
	}

	if (!m_paused) {
		m_3dManager->GetLego3DView()->GetView()->ForceUpdate(p_x, p_y, p_width, p_height);
	}
}

// FUNCTION: LEGO1 0x1007c520
void LegoVideoManager::FUN_1007c520()
{
	m_unk0xe5 = TRUE;
	m_render3d = FALSE;
	m_videoParam.GetPalette()->SetOverrideSkyColor(FALSE);

	m_displaySurface->ClearScreen();
	InputManager()->EnableInputProcessing();
	InputManager()->SetUnknown335(TRUE);
}

extern void ViewportDestroyCallback(IDirect3DRMObject*, void*);

// FUNCTION: LEGO1 0x1007c560
int LegoVideoManager::EnableRMDevice()
{
	IDirect3DRMViewport* viewport;

	if (!m_paused) {
		return -1;
	}

	TglImpl::DeviceImpl* deviceImpl = (TglImpl::DeviceImpl*) m_3dManager->GetLego3DView()->GetDevice();
	IDirect3DRMDevice2* d3drmDev2 = NULL;
	IDirect3D2* d3d2 = m_direct3d->Direct3D();
	IDirect3DDevice2* d3dDev2 = m_direct3d->Direct3DDevice();

	int result = -1;
	IDirect3DRM2* d3drm2 = ((TglImpl::RendererImpl*) m_renderer)->ImplementationData();

	m_direct3d->RestoreSurfaces();

	if (d3drm2->CreateDeviceFromD3D(d3d2, d3dDev2, &d3drmDev2) == D3DRM_OK) {
		viewport = NULL;
		deviceImpl->SetImplementationData(d3drmDev2);

		if (d3drm2->CreateViewport(d3drmDev2, m_camera, 0, 0, m_cameraWidth, m_cameraHeight, &viewport) == D3DRM_OK) {
			viewport->SetBack(m_back);
			viewport->SetFront(m_front);
			viewport->SetField(m_fov);
			viewport->SetCamera(m_camera);
			viewport->SetProjection(m_projection);
			viewport->SetAppData((LPD3DRM_APPDATA) m_appdata);
			d3drmDev2->SetQuality(m_quality);
			d3drmDev2->SetShades(m_shades);
			d3drmDev2->SetTextureQuality(m_textureQuality);
			d3drmDev2->SetRenderMode(m_rendermode);
			d3drmDev2->SetDither(m_dither);
			d3drmDev2->SetBufferCount(m_bufferCount);
			m_camera->Release();

			if (viewport->AddDestroyCallback(ViewportDestroyCallback, m_appdata) == D3DRM_OK) {
				((TglImpl::ViewImpl*) m_3dManager->GetLego3DView()->GetView())->SetImplementationData(viewport);
				m_paused = 0;
				result = 0;
			}
		}
	}

	return result;
}

// FUNCTION: LEGO1 0x1007c740
int LegoVideoManager::DisableRMDevice()
{
	if (m_paused) {
		return -1;
	}

	IDirect3DRMDevice2* d3drmDev2 =
		((TglImpl::DeviceImpl*) m_3dManager->GetLego3DView()->GetDevice())->ImplementationData();

	if (d3drmDev2 != NULL) {
		IDirect3DRMViewportArray* viewportArray = NULL;

		if (d3drmDev2->GetViewports(&viewportArray) == D3DRM_OK && viewportArray != NULL) {
			if (viewportArray->GetSize() == 1) {
				IDirect3DRMViewport* viewport = NULL;

				if (viewportArray->GetElement(0, &viewport) == D3DRM_OK) {
					m_back = viewport->GetBack();
					m_front = viewport->GetFront();
					m_cameraWidth = viewport->GetWidth();
					m_cameraHeight = viewport->GetHeight();
					m_fov = viewport->GetField();
					viewport->GetCamera(&m_camera);
					m_projection = viewport->GetProjection();
					m_appdata = (ViewportAppData*) viewport->GetAppData();
					viewportArray->Release();
					viewport->Release();
					viewport->DeleteDestroyCallback(ViewportDestroyCallback, this->m_appdata);
					viewport->Release();
					m_paused = 1;
					m_direct3d->Direct3D()->AddRef();
					m_direct3d->Direct3DDevice()->AddRef();
				}
				else {
					viewportArray->Release();
				}
			}
		}

		m_quality = d3drmDev2->GetQuality();
		m_shades = d3drmDev2->GetShades();
		m_textureQuality = d3drmDev2->GetTextureQuality();
		m_rendermode = d3drmDev2->GetRenderMode();
		m_dither = d3drmDev2->GetDither();
		m_bufferCount = d3drmDev2->GetBufferCount();
		d3drmDev2->Release();
	}

	if (m_paused) {
		return 0;
	}
	else {
		return -1;
	}
}

// FUNCTION: LEGO1 0x1007c930
MxResult LegoVideoManager::ConfigureD3DRM()
{
	IDirect3DRMDevice2* d3drm =
		((TglImpl::DeviceImpl*) m_3dManager->GetLego3DView()->GetDevice())->ImplementationData();

	if (!d3drm) {
		return FAILURE;
	}

	MxAssignedDevice* assignedDevice = m_direct3d->AssignedDevice();

	if (assignedDevice && assignedDevice->GetFlags() & MxAssignedDevice::c_hardwareMode) {
		if ((assignedDevice->GetDesc().dpcTriCaps.dwTextureFilterCaps & D3DPTFILTERCAPS_LINEAR) ==
			D3DPTFILTERCAPS_LINEAR) {
			d3drm->SetTextureQuality(D3DRMTEXTURE_LINEAR);
		}

		d3drm->SetDither(TRUE);

		if ((assignedDevice->GetDesc().dpcTriCaps.dwShadeCaps & D3DPSHADECAPS_ALPHAFLATBLEND) ==
			D3DPSHADECAPS_ALPHAFLATBLEND) {
			d3drm->SetRenderMode(D3DRMRENDERMODE_BLENDEDTRANSPARENCY);
		}
	}

	return SUCCESS;
}

void LegoVideoManager::DrawDigitToBuffer32(uint8_t* p_dst, int p_pitch, int p_x, int p_y, int p_digit, uint32_t p_color)
{
	if (p_digit < 0 || p_digit > 9) {
		return;
	}

	uint32_t* pixels = (uint32_t*) p_dst;
	int rowStride = p_pitch / 4;

	// 4x5 bitmap font
	const uint8_t digitFont[5][10] = {
		{0b1111, 0b0001, 0b1111, 0b1111, 0b1001, 0b1111, 0b1111, 0b1111, 0b1111, 0b1111},
		{0b1001, 0b0001, 0b0001, 0b0001, 0b1001, 0b1000, 0b1000, 0b0001, 0b1001, 0b1001},
		{0b1001, 0b0001, 0b1111, 0b1111, 0b1111, 0b1111, 0b1111, 0b0010, 0b1111, 0b1111},
		{0b1001, 0b0001, 0b1000, 0b0001, 0b0001, 0b0001, 0b1001, 0b0010, 0b1001, 0b0001},
		{0b1111, 0b0001, 0b1111, 0b1111, 0b0001, 0b1111, 0b1111, 0b0100, 0b1111, 0b1111},
	};

	for (int row = 0; row < 5; ++row) {
		uint8_t bits = digitFont[row][p_digit];
		for (int col = 0; col < 5; ++col) {
			if (bits & (1 << (4 - col))) {
				for (int dy = 0; dy < 2; ++dy) {
					for (int dx = 0; dx < 2; ++dx) {
						pixels[(p_y + row * 2 + dy) * rowStride + (p_x + col * 2 + dx)] = p_color;
					}
				}
			}
		}
	}
}

void LegoVideoManager::DrawTextToSurface32(
	uint8_t* p_dst,
	int p_pitch,
	int p_x,
	int p_y,
	const char* p_text,
	uint32_t p_color
)
{
	while (*p_text) {
		if (*p_text >= '0' && *p_text <= '9') {
			DrawDigitToBuffer32(p_dst, p_pitch, p_x, p_y, *p_text - '0', p_color);
			p_x += 10;
		}
		else if (*p_text == '.') {
			uint32_t* pixels = (uint32_t*) p_dst;
			int rowStride = p_pitch / 4;
			for (int dy = 0; dy < 2; ++dy) {
				for (int dx = 0; dx < 2; ++dx) {
					pixels[(p_y + 10 + dy) * rowStride + (p_x + 2 + dx)] = p_color;
				}
			}
			p_x += 4;
		}
		++p_text;
	}
}

void LegoVideoManager::SetCursorBitmap(const CursorBitmap* p_cursorBitmap)
{
	if (p_cursorBitmap == NULL) {
		m_drawCursor = FALSE;
		return;
	}

	if (m_cursorSurface != NULL) {
		m_cursorSurface->Release();
		m_cursorSurface = NULL;
	}

	m_cursorRect.top = 0;
	m_cursorRect.left = 0;
	m_cursorRect.bottom = p_cursorBitmap->height;
	m_cursorRect.right = p_cursorBitmap->width;

	m_cursorSurface = MxDisplaySurface::CreateCursorSurface(p_cursorBitmap, m_videoParam.GetPalette());

	if (m_cursorSurface == NULL) {
		m_drawCursor = FALSE;
		return;
	}

	m_drawCursor = TRUE;
}
