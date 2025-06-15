#pragma once

#include "d3drmobject_impl.h"
#include "miniwin/ddraw.h"

#include <SDL3/SDL.h>

struct Direct3DRMTextureImpl : public Direct3DRMObjectBaseImpl<IDirect3DRMTexture2>, public IDirectDrawSurface3 {
	Direct3DRMTextureImpl(int width, int height, SDL_PixelFormat format);
	~Direct3DRMTextureImpl() override;

	// IUnknown interface
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override;
	// IDirect3DRMTexture2 interface
	HRESULT Changed(BOOL pixels, BOOL palette) override;
	// IDirectDrawSurface interface
	HRESULT AddAttachedSurface(IDirectDrawSurface* lpDDSAttachedSurface) override;
	HRESULT Blt(
		LPRECT lpDestRect,
		IDirectDrawSurface* lpDDSrcSurface,
		LPRECT lpSrcRect,
		DDBltFlags dwFlags,
		LPDDBLTFX lpDDBltFx
	) override;
	HRESULT BltFast(DWORD dwX, DWORD dwY, IDirectDrawSurface* lpDDSrcSurface, LPRECT lpSrcRect, DDBltFastFlags dwTrans)
		override;
	HRESULT Flip(IDirectDrawSurface* lpDDSurfaceTargetOverride, DDFlipFlags dwFlags) override;
	HRESULT GetAttachedSurface(LPDDSCAPS lpDDSCaps, IDirectDrawSurface** lplpDDAttachedSurface) override;
	HRESULT GetDC(HDC* lphDC) override;
	HRESULT GetPalette(LPDIRECTDRAWPALETTE* lplpDDPalette) override;
	HRESULT GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) override;
	HRESULT GetSurfaceDesc(DDSURFACEDESC* lpDDSurfaceDesc) override;
	HRESULT IsLost() override;
	HRESULT Lock(LPRECT lpDestRect, DDSURFACEDESC* lpDDSurfaceDesc, DDLockFlags dwFlags, HANDLE hEvent) override;
	HRESULT ReleaseDC(HDC hDC) override;
	HRESULT Restore() override;
	HRESULT SetClipper(IDirectDrawClipper* lpDDClipper) override;
	HRESULT SetColorKey(DDColorKeyFlags dwFlags, LPDDCOLORKEY lpDDColorKey) override;
	HRESULT SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) override;
	HRESULT Unlock(LPVOID lpSurfaceData) override;

	SDL_Surface* m_surface = nullptr;
	Uint8 m_version = 0;

private:
	IDirectDrawPalette* m_palette = nullptr;
};
