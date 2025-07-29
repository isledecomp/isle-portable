#include "miniwin/d3drm.h"

#include "d3drm_impl.h"
#include "d3drmdevice_impl.h"
#include "d3drmframe_impl.h"
#include "d3drmlight_impl.h"
#include "d3drmmesh_impl.h"
#include "d3drmobject_impl.h"
#include "d3drmrenderer.h"
#include "d3drmtexture_impl.h"
#include "d3drmviewport_impl.h"
#include "ddraw_impl.h"
#include "ddsurface_impl.h"
#include "miniwin.h"

#include <SDL3/SDL.h>

Direct3DRMPickedArrayImpl::Direct3DRMPickedArrayImpl(const PickRecord* inputPicks, size_t count)
{
	picks.reserve(count);
	for (size_t i = 0; i < count; ++i) {
		const PickRecord& pick = inputPicks[i];
		if (pick.visual) {
			pick.visual->AddRef();
		}
		if (pick.frameArray) {
			pick.frameArray->AddRef();
		}
		picks.push_back(pick);
	}
}

Direct3DRMPickedArrayImpl::~Direct3DRMPickedArrayImpl()
{
	for (PickRecord& pick : picks) {
		if (pick.visual) {
			pick.visual->Release();
		}
		if (pick.frameArray) {
			pick.frameArray->Release();
		}
	}
}

DWORD Direct3DRMPickedArrayImpl::GetSize()
{
	return static_cast<DWORD>(picks.size());
}

HRESULT Direct3DRMPickedArrayImpl::GetPick(
	DWORD index,
	IDirect3DRMVisual** visual,
	IDirect3DRMFrameArray** frameArray,
	D3DRMPICKDESC* desc
)
{
	if (index >= picks.size()) {
		return DDERR_INVALIDPARAMS;
	}

	const PickRecord& pick = picks[index];

	*visual = pick.visual;
	*frameArray = pick.frameArray;
	*desc = pick.desc;

	if (*visual) {
		(*visual)->AddRef();
	}
	if (*frameArray) {
		(*frameArray)->AddRef();
	}

	return DD_OK;
}

struct Direct3DRMWinDeviceImpl : public IDirect3DRMWinDevice {
	HRESULT Activate() override
	{
		MINIWIN_NOT_IMPLEMENTED();
		return DD_OK;
	}
	HRESULT Paint() override
	{
		MINIWIN_NOT_IMPLEMENTED();
		return DD_OK;
	}
	void HandleActivate(WORD wParam) override { MINIWIN_NOT_IMPLEMENTED(); }
	void HandlePaint(void* p_dc) override { MINIWIN_NOT_IMPLEMENTED(); }
};

struct Direct3DRMMaterialImpl : public Direct3DRMObjectBaseImpl<IDirect3DRMMaterial> {
	Direct3DRMMaterialImpl(D3DVALUE power) : m_power(power) {}
	D3DVALUE GetPower() override { return m_power; }

private:
	D3DVALUE m_power;
};

HRESULT Direct3DRMImpl::QueryInterface(const GUID& riid, void** ppvObject)
{
	if (SDL_memcmp(&riid, &IID_IDirect3DRM2, sizeof(GUID)) == 0) {
		this->IUnknown::AddRef();
		*ppvObject = static_cast<IDirect3DRM2*>(this);
		return DD_OK;
	}
	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Direct3DRMImpl does not implement guid");
	return E_NOINTERFACE;
}

HRESULT Direct3DRMImpl::CreateDeviceFromD3D(
	const IDirect3D2* d3d,
	IDirect3DDevice2* d3dDevice,
	IDirect3DRMDevice2** outDevice
)
{
	auto renderer = static_cast<Direct3DRMRenderer*>(d3dDevice);
	*outDevice = static_cast<IDirect3DRMDevice2*>(
		new Direct3DRMDevice2Impl(renderer->GetVirtualWidth(), renderer->GetVirtualHeight(), renderer)
	);
	return DD_OK;
}

HRESULT Direct3DRMImpl::CreateDeviceFromSurface(
	const GUID* guid,
	IDirectDraw* dd,
	IDirectDrawSurface* surface,
	IDirect3DRMDevice2** outDevice
)
{
	DDSURFACEDESC DDSDesc;
	DDSDesc.dwSize = sizeof(DDSURFACEDESC);
	surface->GetSurfaceDesc(&DDSDesc);

	IDirect3DMiniwin* miniwind3d = nullptr;
	dd->QueryInterface(IID_IDirect3DMiniwin, (void**) &miniwind3d);
	SDL_assert(miniwind3d);

	DDRenderer = CreateDirect3DRMRenderer(miniwind3d, DDSDesc, guid);
	if (!DDRenderer) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Device GUID not recognized");
		return E_NOINTERFACE;
	}
	*outDevice =
		static_cast<IDirect3DRMDevice2*>(new Direct3DRMDevice2Impl(DDSDesc.dwWidth, DDSDesc.dwHeight, DDRenderer));
	return DD_OK;
}

HRESULT Direct3DRMImpl::CreateTexture(D3DRMIMAGE* image, IDirect3DRMTexture2** outTexture)
{
	*outTexture = static_cast<IDirect3DRMTexture2*>(new Direct3DRMTextureImpl(image));
	return DD_OK;
}

HRESULT Direct3DRMImpl::CreateTextureFromSurface(LPDIRECTDRAWSURFACE surface, IDirect3DRMTexture2** outTexture)
{
	*outTexture = static_cast<IDirect3DRMTexture2*>(new Direct3DRMTextureImpl(surface, true));
	return DD_OK;
}

HRESULT Direct3DRMImpl::CreateMesh(IDirect3DRMMesh** outMesh)
{
	*outMesh = static_cast<IDirect3DRMMesh*>(new Direct3DRMMeshImpl);
	return DD_OK;
}

HRESULT Direct3DRMImpl::CreateMaterial(D3DVAL power, IDirect3DRMMaterial** outMaterial)
{
	*outMaterial = static_cast<IDirect3DRMMaterial*>(new Direct3DRMMaterialImpl(power));
	return DD_OK;
}

HRESULT Direct3DRMImpl::CreateLightRGB(D3DRMLIGHTTYPE type, D3DVAL r, D3DVAL g, D3DVAL b, IDirect3DRMLight** outLight)
{
	*outLight = static_cast<IDirect3DRMLight*>(new Direct3DRMLightImpl(type, r, g, b));
	return DD_OK;
}

HRESULT Direct3DRMImpl::CreateFrame(IDirect3DRMFrame* parent, IDirect3DRMFrame2** outFrame)
{
	auto parentImpl = static_cast<Direct3DRMFrameImpl*>(parent);
	*outFrame = static_cast<IDirect3DRMFrame2*>(new Direct3DRMFrameImpl{parentImpl});
	return DD_OK;
}

HRESULT Direct3DRMImpl::CreateViewport(
	IDirect3DRMDevice2* iDevice,
	IDirect3DRMFrame* camera,
	int x,
	int y,
	int width,
	int height,
	IDirect3DRMViewport** outViewport
)
{
	auto device = static_cast<Direct3DRMDevice2Impl*>(iDevice);
	auto* viewport = new Direct3DRMViewportImpl(width, height, device->m_renderer);
	if (camera) {
		viewport->SetCamera(camera);
	}
	*outViewport = static_cast<IDirect3DRMViewport*>(viewport);
	device->AddViewport(*outViewport);
	return DD_OK;
}

HRESULT Direct3DRMImpl::SetDefaultTextureShades(DWORD count)
{
	return DD_OK;
}

HRESULT Direct3DRMImpl::SetDefaultTextureColors(DWORD count)
{
	return DD_OK;
}

HRESULT WINAPI Direct3DRMCreate(IDirect3DRM** direct3DRM)
{
	*direct3DRM = new Direct3DRMImpl;
	return DD_OK;
}

D3DCOLOR D3DRMCreateColorRGBA(D3DVALUE red, D3DVALUE green, D3DVALUE blue, D3DVALUE alpha)
{
	int a = static_cast<int>(255.f * alpha);
	int r = static_cast<int>(255.f * red);
	int g = static_cast<int>(255.f * green);
	int b = static_cast<int>(255.f * blue);

	return (a << 24) | (r << 16) | (g << 8) | b;
}
