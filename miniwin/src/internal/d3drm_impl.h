#pragma once

#include "miniwin/d3drm.h"

#include <algorithm>
#include <vector>

struct PickRecord {
	IDirect3DRMVisual* visual;
	IDirect3DRMFrameArray* frameArray;
	D3DRMPICKDESC desc;
};

struct Direct3DRMImpl : virtual public IDirect3DRM2 {
	// IUnknown interface
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override;
	// IDirect3DRM interface
	HRESULT CreateDeviceFromD3D(const IDirect3D2* d3d, IDirect3DDevice2* d3dDevice, IDirect3DRMDevice2** outDevice)
		override;
	HRESULT CreateDeviceFromSurface(
		const GUID* guid,
		IDirectDraw* dd,
		IDirectDrawSurface* surface,
		IDirect3DRMDevice2** outDevice
	) override;
	HRESULT CreateTexture(D3DRMIMAGE* image, IDirect3DRMTexture2** outTexture) override;
	HRESULT CreateTextureFromSurface(IDirectDrawSurface* surface, IDirect3DRMTexture2** outTexture) override;
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

template <typename InterfaceType, typename ActualType, typename ArrayInterface>
class Direct3DRMArrayBaseImpl : public ArrayInterface {
public:
	~Direct3DRMArrayBaseImpl() override
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

struct Direct3DRMPickedArrayImpl : public IDirect3DRMPickedArray {
	Direct3DRMPickedArrayImpl(const PickRecord* inputPicks, size_t count);
	~Direct3DRMPickedArrayImpl() override;
	DWORD GetSize() override;
	HRESULT GetPick(DWORD index, IDirect3DRMVisual** visual, IDirect3DRMFrameArray** frameArray, D3DRMPICKDESC* desc)
		override;

private:
	std::vector<PickRecord> picks;
};
