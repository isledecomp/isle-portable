#include "mxtransitionmanager.h"

#include "legoinputmanager.h"
#include "legoutils.h"
#include "legovideomanager.h"
#include "legoworld.h"
#include "misc.h"
#include "mxbackgroundaudiomanager.h"
#include "mxdisplaysurface.h"
#include "mxmisc.h"
#include "mxparam.h"
#include "mxticklemanager.h"
#include "mxvideopresenter.h"

#include <SDL3/SDL_timer.h>

DECOMP_SIZE_ASSERT(MxTransitionManager, 0x900)

MxTransitionManager::TransitionType g_transitionManagerConfig = MxTransitionManager::e_mosaic;

// GLOBAL: LEGO1 0x100f4378
RECT g_fullScreenRect = {0, 0, 640, 480};

// FUNCTION: LEGO1 0x1004b8d0
// FUNCTION: BETA10 0x100ec2c0
MxTransitionManager::MxTransitionManager()
{
	m_animationTimer = 0;
	m_mode = e_idle;
	m_ddSurface = NULL;
	m_waitIndicator = NULL;
	m_copyBuffer = NULL;
	m_copyFlags.m_bit0 = FALSE;
	m_unk0x28.m_bit0 = FALSE;
	m_unk0x24 = 0;
}

// FUNCTION: LEGO1 0x1004ba00
MxTransitionManager::~MxTransitionManager()
{
	delete[] m_copyBuffer;

	if (m_waitIndicator != NULL) {
		delete m_waitIndicator->GetAction();
		delete m_waitIndicator;
	}

	TickleManager()->UnregisterClient(this);
}

// FUNCTION: LEGO1 0x1004baa0
MxResult MxTransitionManager::GetDDrawSurfaceFromVideoManager() // vtable+0x14
{
	LegoVideoManager* videoManager = VideoManager();
	m_ddSurface = videoManager->GetDisplaySurface()->GetDirectDrawSurface2();
	return SUCCESS;
}

// FUNCTION: LEGO1 0x1004bac0
MxResult MxTransitionManager::Tickle()
{
	Uint64 time = m_animationSpeed + m_systemTime;
	if (time > SDL_GetTicks()) {
		return SUCCESS;
	}

	m_systemTime = SDL_GetTicks();

	switch (m_mode) {
	case e_noAnimation:
		NoTransition();
		break;
	case e_dissolve:
		DissolveTransition();
		break;
	case e_mosaic:
		MosaicTransition();
		break;
	case e_wipeDown:
		WipeDownTransition();
		break;
	case e_windows:
		WindowsTransition();
		break;
	case e_broken:
		BrokenTransition();
		break;
	}
	return SUCCESS;
}

// FUNCTION: LEGO1 0x1004bb70
// FUNCTION: BETA10 0x100ec4c1
MxResult MxTransitionManager::StartTransition(
	TransitionType p_animationType,
	MxS32 p_speed,
	MxBool p_doCopy,
	MxBool p_playMusicInAnim
)
{
#ifdef BETA10
	assert(m_mode == e_idle);
#endif

	if (m_mode == e_idle) {
		if (!p_playMusicInAnim) {
			MxBackgroundAudioManager* backgroundAudioManager = BackgroundAudioManager();
			backgroundAudioManager->Stop();
		}

		m_mode = g_transitionManagerConfig;

		m_copyFlags.m_bit0 = p_doCopy;

		if (m_copyFlags.m_bit0 && m_waitIndicator != NULL) {
			m_waitIndicator->Enable(TRUE);

			MxDSAction* action = m_waitIndicator->GetAction();
			action->SetLoopCount(10000);
			action->SetFlags(action->GetFlags() | MxDSAction::c_bit10);
		}

		Uint64 time = SDL_GetTicks();
		m_systemTime = time;

		m_animationSpeed = p_speed;

		MxTickleManager* tickleManager = TickleManager();
		tickleManager->RegisterClient(this, p_speed);

		LegoInputManager* inputManager = InputManager();
		inputManager->SetUnknown88(TRUE);
		inputManager->SetUnknown336(FALSE);

		LegoVideoManager* videoManager = VideoManager();
		videoManager->SetRender3D(FALSE);

		SetAppCursor(e_cursorBusy);
		return SUCCESS;
	}
	return FAILURE;
}

// FUNCTION: LEGO1 0x1004bc30
void MxTransitionManager::EndTransition(MxBool p_notifyWorld)
{
	if (m_mode != e_idle) {
		m_mode = e_idle;

		m_copyFlags.m_bit0 = FALSE;

		TickleManager()->UnregisterClient(this);

		if (p_notifyWorld) {
			LegoWorld* world = CurrentWorld();

			if (world) {
#ifdef COMPAT_MODE
				{
					MxNotificationParam param(c_notificationTransitioned, this);
					world->Notify(param);
				}
#else
				world->Notify(MxNotificationParam(c_notificationTransitioned, this));
#endif
			}
		}
	}
}

// FUNCTION: LEGO1 0x1004bcf0
void MxTransitionManager::NoTransition()
{
	LegoVideoManager* videoManager = VideoManager();
	videoManager->GetDisplaySurface()->ClearScreen();
	EndTransition(TRUE);
}

// FUNCTION: LEGO1 0x1004bd10
void MxTransitionManager::DissolveTransition()
{
	// If the animation is finished
	if (m_animationTimer == 40) {
		m_animationTimer = 0;
		EndTransition(TRUE);
		return;
	}

	// If we are starting the animation
	if (m_animationTimer == 0) {
		// Generate the list of columns in order...
		MxS32 i;
		for (i = 0; i < 640; i++) {
			m_columnOrder[i] = i;
		}

		// ...then shuffle the list (to ensure that we hit each column once)
		for (i = 0; i < 640; i++) {
			MxS32 swap = SDL_rand(640);
			MxU16 t = m_columnOrder[i];
			m_columnOrder[i] = m_columnOrder[swap];
			m_columnOrder[swap] = t;
		}

		// For each scanline, pick a random X offset
		for (i = 0; i < 480; i++) {
			m_randomShift[i] = SDL_rand(640);
		}
	}

	// Run one tick of the animation
	DDSURFACEDESC ddsd;
	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);

	HRESULT res = m_ddSurface->Lock(NULL, &ddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);
	if (res == DDERR_SURFACELOST) {
		m_ddSurface->Restore();
		res = m_ddSurface->Lock(NULL, &ddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);
	}

	if (res == DD_OK) {
		SubmitCopyRect(&ddsd);

		for (MxS32 col = 0; col < 640; col++) {
			// Select 16 columns on each tick
			if (m_animationTimer * 16 > m_columnOrder[col]) {
				continue;
			}

			if (m_animationTimer * 16 + 15 < m_columnOrder[col]) {
				continue;
			}

			for (MxS32 row = 0; row < 480; row++) {
				// Shift the chosen column a different amount at each scanline.
				// We use the same shift for that scanline each time.
				// By the end, every pixel gets hit.
				MxS32 xShift = (m_randomShift[row] + col) % 640;

				// Set the chosen pixel to black
				if (ddsd.ddpfPixelFormat.dwRGBBitCount == 8) {
					MxU8* surf = (MxU8*) ddsd.lpSurface + ddsd.lPitch * row + xShift;
					*surf = 0;
				}
				else if (ddsd.ddpfPixelFormat.dwRGBBitCount == 16) {
					MxU8* surf = (MxU8*) ddsd.lpSurface + ddsd.lPitch * row + xShift * 2;
					*(MxU16*) surf = 0;
				}
				else {
					MxU8* surf = (MxU8*) ddsd.lpSurface + ddsd.lPitch * row + xShift * 4;
					*(MxU32*) surf = 0xFF000000;
				}
			}
		}

		SetupCopyRect(&ddsd);
		m_ddSurface->Unlock(ddsd.lpSurface);

		if (VideoManager()->GetVideoParam().Flags().GetFlipSurfaces()) {
			LPDIRECTDRAWSURFACE surf = VideoManager()->GetDisplaySurface()->GetDirectDrawSurface1();
			surf->BltFast(0, 0, m_ddSurface, &g_fullScreenRect, DDBLTFAST_WAIT);
		}

		m_animationTimer++;
	}
}

// FUNCTION: LEGO1 0x1004bed0
void MxTransitionManager::MosaicTransition()
{
	static LPDIRECTDRAWSURFACE g_transitionSurface = nullptr;
	static MxU32 g_colors[64][48];

	if (m_animationTimer == 16) {
		m_animationTimer = 0;
		if (g_transitionSurface) {
			g_transitionSurface->Release();
			g_transitionSurface = nullptr;
		}
		EndTransition(TRUE);
		return;
	}
	else {
		if (m_animationTimer == 0) {

			// Same init/shuffle steps as the dissolve transition, except that
			// we are using big blocky pixels and only need 64 columns.
			MxS32 i;
			for (i = 0; i < 64; i++) {
				m_columnOrder[i] = i;
			}

			for (i = 0; i < 64; i++) {
				MxS32 swap = SDL_rand(64);
				MxU16 t = m_columnOrder[i];
				m_columnOrder[i] = m_columnOrder[swap];
				m_columnOrder[swap] = t;
			}

			// The same is true here. We only need 48 rows.
			for (i = 0; i < 48; i++) {
				m_randomShift[i] = SDL_rand(64);
			}

			DDSURFACEDESC srcDesc = {};
			srcDesc.dwSize = sizeof(srcDesc);
			HRESULT lockRes = m_ddSurface->Lock(NULL, &srcDesc, DDLOCK_WAIT | DDLOCK_READONLY, NULL);
			if (lockRes == DDERR_SURFACELOST) {
				m_ddSurface->Restore();
				lockRes = m_ddSurface->Lock(NULL, &srcDesc, DDLOCK_WAIT | DDLOCK_READONLY, NULL);
			}

			if (lockRes != DD_OK) {
				return;
			}

			MxS32 bytesPerPixel = srcDesc.ddpfPixelFormat.dwRGBBitCount / 8;

			for (MxS32 col = 0; col < 64; col++) {
				for (MxS32 row = 0; row < 48; row++) {
					MxS32 xBlock = (m_randomShift[row] + col) % 64;
					MxS32 y = row * 10;
					MxS32 x = xBlock * 10;

					MxU8* pixelPtr = (MxU8*) srcDesc.lpSurface + y * srcDesc.lPitch + x * bytesPerPixel;

					MxU32 pixel;
					switch (bytesPerPixel) {
					case 1:
						pixel = *pixelPtr;
						break;
					case 2:
						pixel = *(MxU16*) pixelPtr;
						break;
					default:
						pixel = *(MxU32*) pixelPtr;
						break;
					}

					g_colors[col][row] = pixel;
				}
			}

			m_ddSurface->Unlock(srcDesc.lpSurface);

			DDSURFACEDESC mainDesc = {};
			mainDesc.dwSize = sizeof(mainDesc);
			if (m_ddSurface->GetSurfaceDesc(&mainDesc) != DD_OK) {
				return;
			}

			DDSURFACEDESC tempDesc = {};
			tempDesc.dwSize = sizeof(tempDesc);
			tempDesc.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_CAPS;
			tempDesc.dwWidth = 64;
			tempDesc.dwHeight = 48;
			tempDesc.ddpfPixelFormat = mainDesc.ddpfPixelFormat;
			tempDesc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;

			if (MVideoManager()->GetDirectDraw()->CreateSurface(&tempDesc, &g_transitionSurface, nullptr) != DD_OK) {
				return;
			}

			DWORD fillColor = 0x00000000;
			switch (mainDesc.ddpfPixelFormat.dwRGBBitCount) {
			case 8:
				fillColor = 0x10;
				break;
			case 16:
				fillColor = RGB555_CREATE(0x1f, 0, 0x1f);
				break;
			}

			DDBLTFX bltFx = {};
			bltFx.dwSize = sizeof(bltFx);
			bltFx.dwFillColor = fillColor;
			g_transitionSurface->Blt(NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &bltFx);

			DDCOLORKEY key = {};
			key.dwColorSpaceLowValue = key.dwColorSpaceHighValue = fillColor;
			g_transitionSurface->SetColorKey(DDCKEY_SRCBLT, &key);
		}

		// Run one tick of the animation
		DDSURFACEDESC ddsd;
		memset(&ddsd, 0, sizeof(ddsd));
		ddsd.dwSize = sizeof(ddsd);

		HRESULT res = g_transitionSurface->Lock(NULL, &ddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);
		if (res == DDERR_SURFACELOST) {
			g_transitionSurface->Restore();
			res = g_transitionSurface->Lock(NULL, &ddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);
		}

		if (res == DD_OK) {
			SubmitCopyRect(&ddsd);

			// Combine xShift with this value to target the correct location in the buffer.
			MxS32 bytesPerPixel = ddsd.ddpfPixelFormat.dwRGBBitCount / 8;

			for (MxS32 col = 0; col < 64; col++) {
				// Select 4 columns on each tick
				if (m_animationTimer * 4 > m_columnOrder[col]) {
					continue;
				}

				if (m_animationTimer * 4 + 3 < m_columnOrder[col]) {
					continue;
				}

				for (MxS32 row = 0; row < 48; row++) {
					MxS32 x = (m_randomShift[row] + col) % 64;
					MxU8* dest = (MxU8*) ddsd.lpSurface + row * ddsd.lPitch + x * bytesPerPixel;

					MxU32 source = g_colors[col][row];
					switch (bytesPerPixel) {
					case 1:
						*dest = (MxU8) source;
						break;
					case 2:
						*((MxU16*) dest) = (MxU16) source;
						break;
					default:
						*((MxU32*) dest) = source;
						break;
					}
				}
			}

			SetupCopyRect(&ddsd);
			g_transitionSurface->Unlock(ddsd.lpSurface);

			RECT srcRect = {0, 0, 64, 48};
			m_ddSurface->Blt(&g_fullScreenRect, g_transitionSurface, &srcRect, DDBLT_WAIT | DDBLT_KEYSRC, NULL);

			m_animationTimer++;
		}
	}
}

// FUNCTION: LEGO1 0x1004c170
void MxTransitionManager::WipeDownTransition()
{
	// If the animation is finished
	if (m_animationTimer == 240) {
		m_animationTimer = 0;
		EndTransition(TRUE);
		return;
	}

	RECT fillRect = g_fullScreenRect;
	// For each of the 240 animation ticks, blank out two scanlines
	// starting at the top of the screen.
	fillRect.bottom = 2 * (m_animationTimer + 1);

	DDBLTFX bltFx = {};
	bltFx.dwSize = sizeof(bltFx);
	bltFx.dwFillColor = 0xFF000000;

	m_ddSurface->Blt(&fillRect, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &bltFx);

	m_animationTimer++;
}

// FUNCTION: LEGO1 0x1004c270
void MxTransitionManager::WindowsTransition()
{
	if (m_animationTimer == 240) {
		m_animationTimer = 0;
		EndTransition(TRUE);
		return;
	}

	DDBLTFX bltFx = {};
	bltFx.dwSize = sizeof(bltFx);
	bltFx.dwFillColor = 0xFF000000;

	int top = m_animationTimer;
	int bottom = 480 - m_animationTimer - 1;
	int left = m_animationTimer;
	int right = 639 - m_animationTimer;

	RECT topRect = {0, top, 640, top + 1};
	m_ddSurface->Blt(&topRect, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &bltFx);

	RECT bottomRect = {0, bottom, 640, bottom + 1};
	m_ddSurface->Blt(&bottomRect, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &bltFx);

	RECT leftRect = {left, top + 1, left + 1, bottom};
	m_ddSurface->Blt(&leftRect, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &bltFx);

	RECT rightRect = {right, top + 1, right + 1, bottom};
	m_ddSurface->Blt(&rightRect, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &bltFx);

	m_animationTimer++;
}

// FUNCTION: LEGO1 0x1004c3e0
void MxTransitionManager::BrokenTransition()
{
	// This function has no actual animation logic.
	// It also never calls EndTransition to
	// properly terminate the transition, so
	// the game just hangs forever.

	DDSURFACEDESC ddsd;
	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);

	HRESULT res = m_ddSurface->Lock(NULL, &ddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);
	if (res == DDERR_SURFACELOST) {
		m_ddSurface->Restore();
		res = m_ddSurface->Lock(NULL, &ddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);
	}

	if (res == DD_OK) {
		SubmitCopyRect(&ddsd);
		SetupCopyRect(&ddsd);
		m_ddSurface->Unlock(ddsd.lpSurface);
	}
}

// FUNCTION: LEGO1 0x1004c470
void MxTransitionManager::SetWaitIndicator(MxVideoPresenter* p_waitIndicator)
{
	// End current wait indicator
	if (m_waitIndicator != NULL) {
		m_waitIndicator->GetAction()->SetFlags(m_waitIndicator->GetAction()->GetFlags() & ~MxDSAction::c_world);
		m_waitIndicator->EndAction();
		m_waitIndicator = NULL;
	}

	// Check if we were given a new wait indicator
	if (p_waitIndicator != NULL) {
		// Setup the new wait indicator
		m_waitIndicator = p_waitIndicator;

		LegoVideoManager* videoManager = VideoManager();
		videoManager->UnregisterPresenter(*m_waitIndicator);

		if (m_waitIndicator->GetCurrentTickleState() < MxPresenter::e_streaming) {
			m_waitIndicator->Tickle();
		}
	}
	else {
		// Disable copy rect
		m_copyFlags.m_bit0 = FALSE;
	}
}

// FUNCTION: LEGO1 0x1004c4d0
void MxTransitionManager::SubmitCopyRect(LPDDSURFACEDESC p_ddsc)
{
	// Check if the copy rect is setup
	if (m_copyFlags.m_bit0 == FALSE || m_waitIndicator == NULL || m_copyBuffer == NULL) {
		return;
	}

	// Copy the copy rect onto the surface
	MxU8* dst;

	MxU32 bytesPerPixel = p_ddsc->ddpfPixelFormat.dwRGBBitCount / 8;

	const MxU8* src = (const MxU8*) m_copyBuffer;

	MxS32 copyPitch;
	copyPitch = ((m_copyRect.right - m_copyRect.left) + 1) * bytesPerPixel;

	MxS32 y;
	dst = (MxU8*) p_ddsc->lpSurface + (p_ddsc->lPitch * m_copyRect.top) + (bytesPerPixel * m_copyRect.left);

	for (y = 0; y < m_copyRect.bottom - m_copyRect.top + 1; ++y) {
		memcpy(dst, src, copyPitch);
		src += copyPitch;
		dst += p_ddsc->lPitch;
	}

	// Free the copy buffer
	delete[] m_copyBuffer;
	m_copyBuffer = NULL;
}

// FUNCTION: LEGO1 0x1004c580
void MxTransitionManager::SetupCopyRect(LPDDSURFACEDESC p_ddsc)
{
	// Check if the copy rect is setup
	if (m_copyFlags.m_bit0 == FALSE || m_waitIndicator == NULL) {
		return;
	}

	// Tickle wait indicator
	m_waitIndicator->Tickle();

	// Check if wait indicator has started
	if (m_waitIndicator->GetCurrentTickleState() >= MxPresenter::e_streaming) {
		// Setup the copy rect
		MxU32 copyPitch = (p_ddsc->ddpfPixelFormat.dwRGBBitCount / 8) *
						  (m_copyRect.right - m_copyRect.left + 1); // This uses m_copyRect, seemingly erroneously
		MxU32 bytesPerPixel = p_ddsc->ddpfPixelFormat.dwRGBBitCount / 8;

		m_copyRect.left = m_waitIndicator->GetLocation().GetX();
		m_copyRect.top = m_waitIndicator->GetLocation().GetY();

		MxS32 height = m_waitIndicator->GetHeight();
		MxS32 width = m_waitIndicator->GetWidth();

		m_copyRect.right = m_copyRect.left + width - 1;
		m_copyRect.bottom = m_copyRect.top + height - 1;

		// Allocate the copy buffer
		const MxU8* src =
			(const MxU8*) p_ddsc->lpSurface + m_copyRect.top * p_ddsc->lPitch + bytesPerPixel * m_copyRect.left;

		m_copyBuffer = new MxU8[bytesPerPixel * width * height];
		if (!m_copyBuffer) {
			return;
		}

		// Copy into the copy buffer
		MxU8* dst = m_copyBuffer;

		for (MxS32 i = 0; i < (m_copyRect.bottom - m_copyRect.top + 1); i++) {
			memcpy(dst, src, copyPitch);
			src += p_ddsc->lPitch;
			dst += copyPitch;
		}
	}

	// Setup display surface
	if ((m_waitIndicator->GetAction()->GetFlags() & MxDSAction::c_bit5) != 0) {
		MxDisplaySurface* displaySurface = VideoManager()->GetDisplaySurface();
		MxBool und = FALSE;
		displaySurface->VTable0x2c(
			p_ddsc,
			m_waitIndicator->GetBitmap(),
			0,
			0,
			m_waitIndicator->GetLocation().GetX(),
			m_waitIndicator->GetLocation().GetY(),
			m_waitIndicator->GetWidth(),
			m_waitIndicator->GetHeight(),
			und
		);
	}
	else {
		MxDisplaySurface* displaySurface = VideoManager()->GetDisplaySurface();
		displaySurface->VTable0x24(
			p_ddsc,
			m_waitIndicator->GetBitmap(),
			0,
			0,
			m_waitIndicator->GetLocation().GetX(),
			m_waitIndicator->GetLocation().GetY(),
			m_waitIndicator->GetWidth(),
			m_waitIndicator->GetHeight()
		);
	}
}

void MxTransitionManager::configureMxTransitionManager(TransitionType p_transitionManagerConfig)
{
	g_transitionManagerConfig = p_transitionManagerConfig;
}
