#pragma once

#include "miniwin_d3drm.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <vector>

extern SDL_Window* DDWindow_SDL3GPU;
extern SDL_Surface* DDBackBuffer_SDL3GPU;

struct PickRecord {
	IDirect3DRMVisual* visual;
	IDirect3DRMFrameArray* frameArray;
	D3DRMPICKDESC desc;
};

struct Direct3DRM_SDL3GPUImpl : virtual public IDirect3DRM2 {
	// IUnknown interface
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override;
	// IDirect3DRM interface
	HRESULT CreateDevice(IDirect3DRMDevice2** outDevice, DWORD width, DWORD height);
	HRESULT CreateDeviceFromD3D(const IDirect3D2* d3d, IDirect3DDevice2* d3dDevice, IDirect3DRMDevice2** outDevice)
		override;
	HRESULT CreateDeviceFromSurface(
		const GUID* guid,
		IDirectDraw* dd,
		IDirectDrawSurface* surface,
		IDirect3DRMDevice2** outDevice
	) override;
	HRESULT CreateTexture(D3DRMIMAGE* image, IDirect3DRMTexture2** outTexture) override;
	HRESULT CreateTextureFromSurface(LPDIRECTDRAWSURFACE surface, IDirect3DRMTexture2** outTexture) override;
	HRESULT CreateMesh(IDirect3DRMMesh** outMesh) override;
	HRESULT CreateMaterial(D3DVAL power, IDirect3DRMMaterial** outMaterial) override;
	HRESULT CreateLightRGB(D3DRMLIGHTTYPE type, D3DVAL r, D3DVAL g, D3DVAL b, IDirect3DRMLight** outLight) override;
	HRESULT CreateFrame(IDirect3DRMFrame* parent, IDirect3DRMFrame2** outFrame) override;
	HRESULT CreateViewport(
		IDirect3DRMDevice2* iDevice,
		IDirect3DRMFrame* camera,
		int x,
		int y,
		int width,
		int height,
		IDirect3DRMViewport** outViewport
	) override;
	HRESULT SetDefaultTextureShades(DWORD count) override;
	HRESULT SetDefaultTextureColors(DWORD count) override;
};

typedef struct PositionColorVertex {
	float x, y, z;
	float nx, ny, nz;
	Uint8 r, g, b, a;
} PositionColorVertex;

template <typename InterfaceType, typename ActualType, typename ArrayInterface>
class Direct3DRMArrayBase_SDL3GPUImpl : public ArrayInterface {
public:
	~Direct3DRMArrayBase_SDL3GPUImpl() override
	{
		for (auto* item : m_items) {
			if (item) {
				item->Release();
			}
		}
	}
	DWORD GetSize() override { return static_cast<DWORD>(m_items.size()); }
	HRESULT AddElement(InterfaceType* in) override
	{
		if (!in) {
			return DDERR_INVALIDPARAMS;
		}
		auto inImpl = static_cast<ActualType*>(in);
		inImpl->AddRef();
		m_items.push_back(inImpl);
		return DD_OK;
	}
	HRESULT GetElement(DWORD index, InterfaceType** out) override
	{
		if (index >= m_items.size()) {
			return DDERR_INVALIDPARAMS;
		}
		*out = static_cast<InterfaceType*>(m_items[index]);
		if (*out) {
			(*out)->AddRef();
		}
		return DD_OK;
	}
	HRESULT DeleteElement(InterfaceType* element) override
	{
		auto it = std::find(m_items.begin(), m_items.end(), element);
		if (it == m_items.end()) {
			return DDERR_INVALIDPARAMS;
		}

		(*it)->Release();
		m_items.erase(it);
		return DD_OK;
	}

protected:
	std::vector<ActualType*> m_items;
};

struct Direct3DRMPickedArray_SDL3GPUImpl : public IDirect3DRMPickedArray {
	Direct3DRMPickedArray_SDL3GPUImpl(const PickRecord* inputPicks, size_t count);
	~Direct3DRMPickedArray_SDL3GPUImpl() override;
	DWORD GetSize() override;
	HRESULT GetPick(DWORD index, IDirect3DRMVisual** visual, IDirect3DRMFrameArray** frameArray, D3DRMPICKDESC* desc)
		override;

private:
	std::vector<PickRecord> picks;
};
