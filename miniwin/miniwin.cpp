#include "miniwin.h"

#include "miniwin_d3d.h"
#include "miniwin_d3drm.h"
#include "miniwin_ddraw.h"

#include <SDL3/SDL_log.h>

bool IsEqualGUID(const GUID& a, const GUID& b)
{
	return a.m_data1 == b.m_data1 && a.m_data2 == b.m_data2 && a.m_data3 == b.m_data3 && a.m_data4 == b.m_data4;
}

HRESULT IUnknown::QueryInterface(const GUID& riid, void** ppvObject)
{
	if (!ppvObject) {
		return DDERR_INVALIDPARAMS;
	}

	if (IsEqualGUID(riid, IID_IDirectDraw2)) {
		*ppvObject = new IDirectDraw2();
		return S_OK;
	}

	if (IsEqualGUID(riid, IID_IDirectDrawSurface3)) {
		*ppvObject = new IDirectDrawSurface3();
		return S_OK;
	}

	if (IsEqualGUID(riid, IID_IDirect3D2)) {
		*ppvObject = new IDirect3D2();
		return S_OK;
	}

	if (IsEqualGUID(riid, IID_IDirect3DRM2)) {
		*ppvObject = new IDirect3DRM2();
		return S_OK;
	}

	if (IsEqualGUID(riid, IID_IDirect3DRMWinDevice)) {
		*ppvObject = new IDirect3DRMWinDevice();
		return S_OK;
	}

	if (IsEqualGUID(riid, IID_IDirect3DRMMesh) ||
		IsEqualGUID(riid, IID_IDirect3DRMMeshBuilder)) { // Work around bug in GroupImpl::Bounds()
		*ppvObject = new IDirect3DRMMesh();
		return S_OK;
	}

	if (IsEqualGUID(riid, IID_IDirect3DRMTexture2)) {
		*ppvObject = new IDirect3DRMTexture2();
		return S_OK;
	}

	return DDERR_INVALIDPARAMS;
}

void OutputDebugString(const char* lpOutputString)
{
	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", lpOutputString);
}
