#pragma once

#include <SDL3/SDL.h>
#include <ddsurface_impl.h>
#include <miniwin/ddraw.h>

#include <SDL3/SDL_gxm.h>
#include <psp2/gxm.h>
#define VITA_GXM_DISPLAY_BUFFER_COUNT 2

typedef struct GXMDisplayBuffer {
	SceUID uid;
	SceGxmSyncObject* sync;
	void* data;
	SceGxmColorSurface surface;
} GXMDisplayBuffer;

typedef struct {
    void *address;
    int width;
	int height;
} GXMDisplayData;

struct FrameBufferImpl : public IDirectDrawSurface3 {
	FrameBufferImpl(LPDDSURFACEDESC lpDDSurfaceDesc);
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
	HRESULT IsLost() override;
	HRESULT Lock(LPRECT lpDestRect, DDSURFACEDESC* lpDDSurfaceDesc, DDLockFlags dwFlags, HANDLE hEvent) override;
	HRESULT ReleaseDC(HDC hDC) override;
	HRESULT Restore() override;
	HRESULT SetClipper(IDirectDrawClipper* lpDDClipper) override;
	HRESULT SetColorKey(DDColorKeyFlags dwFlags, LPDDCOLORKEY lpDDColorKey) override;
	HRESULT SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) override;
	HRESULT Unlock(LPVOID lpSurfaceData) override;

// added
	inline GXMDisplayBuffer* backBuffer() {
		return &displayBuffers[backBufferIndex];
	}
	inline SceGxmRenderTarget* GetRenderTarget() {
		return this->renderTarget;
	}

private:
	SceGxmContext* context;
	SceGxmShaderPatcher* shaderPatcher;
	SceGxmRenderTarget* renderTarget;
	SceClibMspace cdramPool;

	SceGxmShaderPatcherId blitVertexProgramId;
	SceGxmVertexProgram* blitVertexProgram;
	SceGxmShaderPatcherId blitColorFragmentProgramId;
	SceGxmFragmentProgram* blitColorFragmentProgram;
	SceGxmShaderPatcherId blitTexFragmentProgramId;
	SceGxmFragmentProgram* blitTexFragmentProgram;

	const SceGxmProgramParameter* uScreenMatrix;
	const SceGxmProgramParameter* uColor;
	const SceGxmProgramParameter* uTexMatrix;

	void* quadMeshBuffer;
	float* quadVerticies;
	uint16_t* quadIndicies;

	GXMDisplayBuffer displayBuffers[VITA_GXM_DISPLAY_BUFFER_COUNT];
	int backBufferIndex = 0;
	int frontBufferIndex = 1;
	int width;
	int height;
	bool sceneStarted;
	IDirectDrawPalette* m_palette = nullptr;
};
