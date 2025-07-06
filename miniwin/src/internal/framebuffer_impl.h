#pragma once

#include <SDL3/SDL.h>
#include <ddsurface_impl.h>
#include <miniwin/ddraw.h>

struct FrameBufferImpl : public IDirectDrawSurface3 {
	FrameBufferImpl(DWORD virtualWidth, DWORD virtualHeight);
	~FrameBufferImpl() override;

	// IUnknown interface
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override;
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
	HRESULT IsLost() override { return DD_OK; }
	HRESULT Lock(LPRECT lpDestRect, DDSURFACEDESC* lpDDSurfaceDesc, DDLockFlags dwFlags, HANDLE hEvent) override;
	HRESULT ReleaseDC(HDC hDC) override;
	HRESULT Restore() override;
	HRESULT SetClipper(IDirectDrawClipper* lpDDClipper) override { return DD_OK; }
	HRESULT SetColorKey(DDColorKeyFlags dwFlags, LPDDCOLORKEY lpDDColorKey) override;
	HRESULT SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) override;
	HRESULT Unlock(LPVOID lpSurfaceData) override;

private:
	uint32_t m_virtualWidth;
	uint32_t m_virtualHeight;
	bool m_readOnlyLock;
	DirectDrawSurfaceImpl* m_transferBuffer;
	IDirectDrawPalette* m_palette = nullptr;
};
