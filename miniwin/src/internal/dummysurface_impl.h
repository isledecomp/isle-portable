#pragma once

#include <miniwin.h>
#include <miniwin/ddraw.h>

struct DummySurfaceImpl : public IDirectDrawSurface3 {
	// IUnknown interface
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override
	{
		MINIWIN_NOT_IMPLEMENTED();
		return DDERR_GENERIC;
	}

	// IDirectDrawSurface interface
	HRESULT AddAttachedSurface(IDirectDrawSurface* lpDDSAttachedSurface) override
	{
		MINIWIN_NOT_IMPLEMENTED();
		return DDERR_GENERIC;
	}
	HRESULT Blt(
		LPRECT lpDestRect,
		IDirectDrawSurface* lpDDSrcSurface,
		LPRECT lpSrcRect,
		DDBltFlags dwFlags,
		LPDDBLTFX lpDDBltFx
	) override
	{
		MINIWIN_NOT_IMPLEMENTED();
		return DDERR_GENERIC;
	}
	HRESULT BltFast(DWORD dwX, DWORD dwY, IDirectDrawSurface* lpDDSrcSurface, LPRECT lpSrcRect, DDBltFastFlags dwTrans)
		override
	{
		MINIWIN_NOT_IMPLEMENTED();
		return DDERR_GENERIC;
	}
	HRESULT Flip(IDirectDrawSurface* lpDDSurfaceTargetOverride, DDFlipFlags dwFlags) override
	{
		MINIWIN_NOT_IMPLEMENTED();
		return DDERR_GENERIC;
	}
	HRESULT GetAttachedSurface(LPDDSCAPS lpDDSCaps, IDirectDrawSurface** lplpDDAttachedSurface) override
	{
		MINIWIN_NOT_IMPLEMENTED();
		return DDERR_GENERIC;
	}
	HRESULT GetDC(HDC* lphDC) override
	{
		MINIWIN_NOT_IMPLEMENTED();
		return DDERR_GENERIC;
	}
	HRESULT GetPalette(LPDIRECTDRAWPALETTE* lplpDDPalette) override
	{
		MINIWIN_NOT_IMPLEMENTED();
		return DDERR_GENERIC;
	}
	HRESULT GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) override
	{
		MINIWIN_NOT_IMPLEMENTED();
		return DDERR_GENERIC;
	}
	HRESULT GetSurfaceDesc(DDSURFACEDESC* lpDDSurfaceDesc) override
	{
		MINIWIN_NOT_IMPLEMENTED();
		return DDERR_GENERIC;
	}
	HRESULT IsLost() override { return DD_OK; }
	HRESULT Lock(LPRECT lpDestRect, DDSURFACEDESC* lpDDSurfaceDesc, DDLockFlags dwFlags, HANDLE hEvent) override
	{
		MINIWIN_NOT_IMPLEMENTED();
		return DDERR_GENERIC;
	}
	HRESULT ReleaseDC(HDC hDC) override
	{
		MINIWIN_NOT_IMPLEMENTED();
		return DDERR_GENERIC;
	}
	HRESULT Restore() override
	{
		MINIWIN_NOT_IMPLEMENTED();
		return DDERR_GENERIC;
	}
	HRESULT SetClipper(IDirectDrawClipper* lpDDClipper) override
	{
		MINIWIN_NOT_IMPLEMENTED();
		return DDERR_GENERIC;
	}
	HRESULT SetColorKey(DDColorKeyFlags dwFlags, LPDDCOLORKEY lpDDColorKey) override
	{
		MINIWIN_NOT_IMPLEMENTED();
		return DDERR_GENERIC;
	}
	HRESULT SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) override
	{
		MINIWIN_NOT_IMPLEMENTED();
		return DDERR_GENERIC;
	}
	HRESULT Unlock(LPVOID lpSurfaceData) override
	{
		MINIWIN_NOT_IMPLEMENTED();
		return DDERR_GENERIC;
	}
};
