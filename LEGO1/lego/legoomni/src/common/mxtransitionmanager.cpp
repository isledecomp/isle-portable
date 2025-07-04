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
	case e_fakeMosaic:
		FakeMosaicTransition();
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
	if (m_animationTimer == 16) {
		m_animationTimer = 0;
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
		}

		// Run one tick of the animation
		DDSURFACEDESC ddsd;
		memset(&ddsd, 0, sizeof(ddsd));
		ddsd.dwSize = sizeof(ddsd);

		HRESULT res = m_ddSurface->Lock(NULL, &ddsd, DDLOCK_WAIT, NULL);
		if (res == DDERR_SURFACELOST) {
			m_ddSurface->Restore();
			res = m_ddSurface->Lock(NULL, &ddsd, DDLOCK_WAIT, NULL);
		}

		if (res == DD_OK) {
			SubmitCopyRect(&ddsd);

			for (MxS32 col = 0; col < 64; col++) {
				// Select 4 columns on each tick
				if (m_animationTimer * 4 > m_columnOrder[col]) {
					continue;
				}

				if (m_animationTimer * 4 + 3 < m_columnOrder[col]) {
					continue;
				}

				for (MxS32 row = 0; row < 48; row++) {
					// To do the mosaic effect, we subdivide the 640x480 surface into
					// 10x10 pixel blocks. At the chosen block, we sample the top-leftmost
					// color and set the other 99 pixels to that value.

					// First, get the offset of the 10x10 block that we will sample for this row.
					MxS32 xShift = 10 * ((m_randomShift[row] + col) % 64);

					// Combine xShift with this value to target the correct location in the buffer.
					MxS32 bytesPerPixel = ddsd.ddpfPixelFormat.dwRGBBitCount / 8;

					// Seek to the sample position.
					MxU8* source = (MxU8*) ddsd.lpSurface + 10 * row * ddsd.lPitch + bytesPerPixel * xShift;

					// Sample byte or word depending on display mode.
					MxU32 sample;
					switch (bytesPerPixel) {
					case 1:
						sample = *source;
						break;
					case 2:
						sample = *(MxU16*) source;
						break;
					default:
						sample = *(MxU32*) source;
						break;
					}

					// For each of the 10 rows in the 10x10 square:
					for (MxS32 k = 10 * row; k < 10 * row + 10; k++) {
						void* pos = (MxU8*) ddsd.lpSurface + k * ddsd.lPitch + bytesPerPixel * xShift;

						switch (bytesPerPixel) {
						case 1: {
							// Optimization: If the pixel is only one byte, we can use memset
							memset(pos, sample, 10);
							break;
						}
						case 2: {
							MxU16* p = (MxU16*) pos;
							for (MxS32 tt = 0; tt < 10; tt++) {
								p[tt] = (MxU16) sample;
							}
							break;
						}
						default: {
							MxU32* p = (MxU32*) pos;
							for (MxS32 tt = 0; tt < 10; tt++) {
								p[tt] = (MxU32) sample;
							}
							break;
						}
						}
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

int g_colorOffset;
int GetColorIndexWithLocality(int p_col, int p_row)
{
	int islandX = p_col / 8;
	int islandY = p_row / 8; // Dvide screen in 8x6 tiles

	int island = islandY * 8 + islandX; // tile id

	if (SDL_rand(3) > island / 8) {
		return 6 + SDL_rand(2); // emulate sky
	}

	if (SDL_rand(16) > 2) {
		island += SDL_rand(3) - 1 + (SDL_rand(3) - 1) * 8; // blure tiles
	}

	int hash = (island + g_colorOffset) * 2654435761u;
	int scrambled = (hash >> 16) % 32;

	int finalIndex = scrambled + SDL_rand(3) - 1;
	return finalIndex % 32;
}

void MxTransitionManager::FakeMosaicTransition()
{
	if (m_animationTimer == 16) {
		m_animationTimer = 0;
		EndTransition(TRUE);
		return;
	}

	if (m_animationTimer == 0) {
		g_colorOffset = SDL_rand(32);
		for (MxS32 i = 0; i < 64; i++) {
			m_columnOrder[i] = i;
		}
		for (MxS32 i = 0; i < 64; i++) {
			MxS32 swap = SDL_rand(64);
			std::swap(m_columnOrder[i], m_columnOrder[swap]);
		}
		for (MxS32 i = 0; i < 48; i++) {
			m_randomShift[i] = SDL_rand(64);
		}
	}

	DDSURFACEDESC ddsd = {};
	ddsd.dwSize = sizeof(ddsd);
	HRESULT res = m_ddSurface->Lock(NULL, &ddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);
	if (res == DDERR_SURFACELOST) {
		m_ddSurface->Restore();
		res = m_ddSurface->Lock(NULL, &ddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);
	}

	if (res == DD_OK) {
		SubmitCopyRect(&ddsd);

		static const MxU8 g_palette[32][3] = {
			{0x00, 0x00, 0x00}, {0x12, 0x1e, 0x50}, {0x00, 0x22, 0x6c}, {0x14, 0x2d, 0x9f}, {0x0e, 0x36, 0xb0},
			{0x0e, 0x39, 0xd0}, {0x47, 0x96, 0xe2}, {0x79, 0xaa, 0xca}, {0xff, 0xff, 0xff}, {0xc9, 0xcd, 0xcb},
			{0xad, 0xad, 0xab}, {0xa6, 0x91, 0x8e}, {0xaf, 0x59, 0x49}, {0xc0, 0x00, 0x00}, {0xab, 0x18, 0x18},
			{0x61, 0x0c, 0x0c}, {0x04, 0x38, 0x12}, {0x2c, 0x67, 0x28}, {0x4a, 0xb4, 0x6b}, {0x94, 0xb7, 0x7c},
			{0xb6, 0xb9, 0x87}, {0x52, 0x4a, 0x67}, {0x87, 0x8d, 0x8a}, {0xa6, 0x91, 0x8e}, {0xf8, 0xee, 0xdc},
			{0xf4, 0xe2, 0xc3}, {0x87, 0x8d, 0x8a}, {0xba, 0x9f, 0x12}, {0xb5, 0x83, 0x00}, {0x6a, 0x44, 0x27},
			{0x36, 0x37, 0x34}, {0x2b, 0x23, 0x0f}
		};

		MxS32 bytesPerPixel = ddsd.ddpfPixelFormat.dwRGBBitCount / 8;

		for (MxS32 col = 0; col < 64; col++) {
			if (m_animationTimer * 4 > m_columnOrder[col]) {
				continue;
			}
			if (m_animationTimer * 4 + 3 < m_columnOrder[col]) {
				continue;
			}

			for (MxS32 row = 0; row < 48; row++) {
				MxS32 xShift = 10 * ((m_randomShift[row] + col) % 64);
				MxS32 yStart = 10 * row;

				int paletteIndex = GetColorIndexWithLocality(xShift / 10, row);

				const MxU8* color = g_palette[paletteIndex];

				for (MxS32 y = 0; y < 10; y++) {
					MxU8* dest = (MxU8*) ddsd.lpSurface + (yStart + y) * ddsd.lPitch + xShift * bytesPerPixel;
					switch (bytesPerPixel) {
					case 1:
						memset(dest, paletteIndex, 10);
						break;
					case 2: {
						MxU32 pixel = RGB555_CREATE(color[2], color[1], color[0]);
						MxU16* p = (MxU16*) dest;
						for (MxS32 i = 0; i < 10; i++) {
							p[i] = pixel;
						}
						break;
					}
					default: {
						MxU32 pixel = RGB8888_CREATE(color[2], color[1], color[0], 255);
						MxU32* p = (MxU32*) dest;
						for (MxS32 i = 0; i < 10; i++) {
							p[i] = pixel;
						}
						break;
					}
					}
				}
			}
		}

		SetupCopyRect(&ddsd);
		m_ddSurface->Unlock(ddsd.lpSurface);

		if (VideoManager()->GetVideoParam().Flags().GetFlipSurfaces()) {
			VideoManager()
				->GetDisplaySurface()
				->GetDirectDrawSurface1()
				->BltFast(0, 0, m_ddSurface, &g_fullScreenRect, DDBLTFAST_WAIT);
		}

		m_animationTimer++;
	}
}
