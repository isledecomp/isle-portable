#include "miniwin_d3drm.h"

#include <SDL3/SDL.h>
#include <assert.h>

struct Direct3DRMDevice2Impl : public IDirect3DRMDevice2 {
	unsigned long GetWidth() override { return 640; }
	unsigned long GetHeight() override { return 480; }
	HRESULT SetBufferCount(int count) override { return DD_OK; }
	HRESULT GetBufferCount() override { return DD_OK; }
	HRESULT SetShades(unsigned long shadeCount) override { return DD_OK; }
	HRESULT GetShades() override { return DD_OK; }
	HRESULT SetQuality(D3DRMRENDERQUALITY quality) override { return DD_OK; }
	D3DRMRENDERQUALITY GetQuality() override { return D3DRMRENDERQUALITY::GOURAUD; }
	HRESULT SetDither(int dither) override { return DD_OK; }
	HRESULT GetDither() override { return DD_OK; }
	HRESULT SetTextureQuality(D3DRMTEXTUREQUALITY quality) override { return DD_OK; }
	D3DRMTEXTUREQUALITY GetTextureQuality() override { return D3DRMTEXTUREQUALITY::LINEAR; }
	HRESULT SetRenderMode(D3DRMRENDERMODE mode) override { return DD_OK; }
	D3DRMRENDERMODE GetRenderMode() override { return D3DRMRENDERMODE::BLENDEDTRANSPARENCY; }
	HRESULT Update() override { return DD_OK; }
	HRESULT GetViewports(IDirect3DRMViewportArray** ppViewportArray) override
	{
		assert(false && "unimplemented");
		return DDERR_GENERIC;
	}
};

struct Direct3DRMFrameImpl : public IDirect3DRMFrame2 {
	HRESULT SetAppData(LPD3DRM_APPDATA appData) override
	{
		m_data = appData;
		return DD_OK;
	}
	LPVOID GetAppData() override { return m_data; }
	HRESULT AddChild(IDirect3DRMFrame* child) override
	{
		child->AddRef();
		return DD_OK;
	}
	HRESULT DeleteChild(IDirect3DRMFrame* child) override
	{
		child->Release();
		return DD_OK;
	}
	HRESULT SetSceneBackgroundRGB(float r, float g, float b) override { return DD_OK; }
	HRESULT AddLight(IDirect3DRMLight* light) override
	{
		light->AddRef();
		return DD_OK;
	}
	HRESULT GetLights(IDirect3DRMLightArray** lightArray) override
	{
		assert(false && "unimplemented");
		return DDERR_GENERIC;
	}
	HRESULT AddTransform(D3DRMCOMBINETYPE combine, D3DRMMATRIX4D matrix) override { return DD_OK; }
	HRESULT GetPosition(int index, D3DVECTOR* position) override { return DD_OK; }
	HRESULT AddVisual(IDirect3DRMVisual* visual) override
	{
		visual->AddRef();
		return DD_OK;
	}
	HRESULT DeleteVisual(IDirect3DRMVisual* visual) override
	{
		visual->Release();
		return DD_OK;
	}
	HRESULT AddVisual(IDirect3DRMMesh* visual) override
	{
		visual->AddRef();
		return DD_OK;
	}
	HRESULT DeleteVisual(IDirect3DRMMesh* visual) override
	{
		visual->Release();
		return DD_OK;
	}
	HRESULT AddVisual(IDirect3DRMFrame* visual) override
	{
		visual->AddRef();
		return DD_OK;
	}
	HRESULT DeleteVisual(IDirect3DRMFrame* visual) override
	{
		visual->Release();
		return DD_OK;
	}
	HRESULT GetVisuals(IDirect3DRMVisualArray** visuals) override
	{
		assert(false && "unimplemented");
		return DDERR_GENERIC;
	}
	HRESULT SetTexture(const IDirect3DRMTexture* texture) override { return DD_OK; }
	HRESULT GetTexture(IDirect3DRMTexture** texture) override
	{
		assert(false && "unimplemented");
		return DDERR_GENERIC;
	}
	HRESULT SetColor(float r, float g, float b, float a) override { return DD_OK; }
	HRESULT SetColor(D3DCOLOR) override { return DD_OK; }
	HRESULT SetColorRGB(float r, float g, float b) override { return DD_OK; }
	HRESULT SetMaterialMode(D3DRMMATERIALMODE mode) override { return DD_OK; }
	HRESULT GetChildren(IDirect3DRMFrameArray** children) override
	{
		assert(false && "unimplemented");
		return DDERR_GENERIC;
	}

private:
	LPD3DRM_APPDATA m_data;
};

struct Direct3DRMViewportImpl : public IDirect3DRMViewport {
	Direct3DRMViewportImpl() : m_data(nullptr) {}
	HRESULT Clone(void** ppObject) override
	{
		assert(false && "unimplemented");
		return DDERR_GENERIC;
	}
	HRESULT AddDestroyCallback(void (*cb)(IDirect3DRMObject*, void*), void* arg) override { return DD_OK; }
	HRESULT DeleteDestroyCallback(void (*cb)(IDirect3DRMObject*, void*), void* arg) override { return DD_OK; }
	HRESULT SetAppData(LPD3DRM_APPDATA appData) override
	{
		m_data = appData;
		return DD_OK;
	}
	LPVOID GetAppData() override { return m_data; }
	HRESULT SetName(const char* name) override { return DD_OK; }
	HRESULT GetName(DWORD* size, char* name) override { return DD_OK; }
	HRESULT GetClassName(DWORD* size, char* name) override { return DD_OK; }
	HRESULT Render(IDirect3DRMFrame* group) override { return DD_OK; }
	HRESULT ForceUpdate(int x, int y, int w, int h) override { return DD_OK; }
	HRESULT Clear() override { return DD_OK; }
	HRESULT SetCamera(IDirect3DRMFrame* camera) override { return DD_OK; }
	HRESULT GetCamera(IDirect3DRMFrame** camera) override
	{
		assert(false && "unimplemented");
		return DDERR_GENERIC;
	}
	HRESULT SetProjection(D3DRMPROJECTIONTYPE type) override { return DD_OK; }
	D3DRMPROJECTIONTYPE GetProjection() override { return D3DRMPROJECTIONTYPE::PERSPECTIVE; }
	HRESULT SetFront(D3DVALUE z) override { return DD_OK; }
	D3DVALUE GetFront() override { return 0; }
	HRESULT SetBack(D3DVALUE z) override { return DD_OK; }
	D3DVALUE GetBack() override { return 0; }
	HRESULT SetField(D3DVALUE field) override { return DD_OK; }
	D3DVALUE GetField() override { return 0; }
	int GetWidth() override { return 640; }
	int GetHeight() override { return 480; }
	HRESULT Transform(D3DRMVECTOR4D* screen, D3DVECTOR* world) override { return DD_OK; }
	HRESULT InverseTransform(D3DVECTOR* world, D3DRMVECTOR4D* screen) override { return DD_OK; }
	HRESULT Pick(float x, float y, LPDIRECT3DRMPICKEDARRAY* pickedArray) override { return DD_OK; }

private:
	LPD3DRM_APPDATA m_data;
};

struct Direct3DRMLightImpl : public IDirect3DRMLight {
	HRESULT SetColorRGB(float r, float g, float b) override { return DD_OK; }
};

struct Direct3DRMImpl : public IDirect3DRM2 {
	// IUnknown interface
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override
	{
		if (SDL_memcmp(&riid, &IID_IDirect3DRM2, sizeof(GUID)) == 0) {
			this->IUnknown::AddRef();
			*ppvObject = static_cast<IDirect3DRM2*>(this);
			return S_OK;
		}
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DirectDrawImpl does not implement guid");
		return E_NOINTERFACE;
	}
	// IDirect3DRM interface
	HRESULT CreateDeviceFromD3D(const IDirect3D2* d3d, IDirect3DDevice2* d3dDevice, IDirect3DRMDevice2** outDevice)
		override
	{
		*outDevice = static_cast<IDirect3DRMDevice2*>(new Direct3DRMDevice2Impl);
		return S_OK;
	}
	HRESULT CreateDeviceFromSurface(
		const GUID* guid,
		IDirectDraw* dd,
		IDirectDrawSurface* surface,
		IDirect3DRMDevice2** outDevice
	) override
	{
		assert(false && "unimplemented");
		return DDERR_GENERIC;
	}
	HRESULT CreateTexture(D3DRMIMAGE* image, IDirect3DRMTexture2** outTexture) override
	{
		assert(false && "unimplemented");
		return DDERR_GENERIC;
	}
	HRESULT CreateTextureFromSurface(LPDIRECTDRAWSURFACE surface, IDirect3DRMTexture2** outTexture) override
	{
		assert(false && "unimplemented");
		return DDERR_GENERIC;
	}
	HRESULT CreateMesh(IDirect3DRMMesh** outMesh) override
	{
		assert(false && "unimplemented");
		return DDERR_GENERIC;
	}
	HRESULT CreateMaterial(D3DVAL power, IDirect3DRMMaterial** outMaterial) override
	{
		*outMaterial = new IDirect3DRMMaterial;
		return DD_OK;
	}
	HRESULT CreateLightRGB(D3DRMLIGHTTYPE type, D3DVAL r, D3DVAL g, D3DVAL b, IDirect3DRMLight** outLight) override
	{
		*outLight = static_cast<IDirect3DRMLight*>(new Direct3DRMLightImpl);
		return DD_OK;
	}
	HRESULT CreateFrame(IDirect3DRMFrame* parent, IDirect3DRMFrame2** outFrame) override
	{
		*outFrame = static_cast<IDirect3DRMFrame2*>(new Direct3DRMFrameImpl);
		return DD_OK;
	}
	HRESULT CreateViewport(
		IDirect3DRMDevice2* device,
		IDirect3DRMFrame* camera,
		int x,
		int y,
		int width,
		int height,
		IDirect3DRMViewport** outViewport
	) override
	{
		*outViewport = static_cast<IDirect3DRMViewport*>(new Direct3DRMViewportImpl);
		return DD_OK;
	}
	HRESULT SetDefaultTextureShades(unsigned int count) override { return DD_OK; }
	HRESULT SetDefaultTextureColors(unsigned int count) override { return DD_OK; }
};

HRESULT WINAPI Direct3DRMCreate(IDirect3DRM** direct3DRM)
{
	*direct3DRM = new Direct3DRMImpl;
	return DD_OK;
}

D3DCOLOR D3DRMCreateColorRGBA(D3DVALUE red, D3DVALUE green, D3DVALUE blue, D3DVALUE alpha)
{
	return 0;
}
