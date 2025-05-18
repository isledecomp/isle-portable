#include "miniwin_d3drm.h"

#include "miniwin_ddsurface_p.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <assert.h>
#include <vector>

template <typename InterfaceType, typename ArrayInterface>
class Direct3DRMArrayBase : public ArrayInterface {
public:
	~Direct3DRMArrayBase() override
	{
		for (auto* item : items) {
			if (item) {
				item->Release();
			}
		}
	}
	DWORD GetSize() override { return static_cast<DWORD>(items.size()); }
	HRESULT AddElement(InterfaceType* in) override
	{
		if (!in) {
			return DDERR_INVALIDPARAMS;
		}
		in->AddRef();
		items.push_back(in);
		return DD_OK;
	}
	HRESULT GetElement(DWORD index, InterfaceType** out) override
	{
		if (index >= items.size()) {
			return DDERR_INVALIDPARAMS;
		}
		*out = static_cast<InterfaceType*>(items[index]);
		if (*out) {
			(*out)->AddRef();
		}
		return DD_OK;
	}
	HRESULT DeleteElement(InterfaceType* element) override
	{
		auto it = std::find(items.begin(), items.end(), element);
		if (it == items.end()) {
			return DDERR_INVALIDPARAMS;
		}

		(*it)->Release();
		items.erase(it);
		return DD_OK;
	}

protected:
	std::vector<InterfaceType*> items;
};

struct Direct3DRMFrameArrayImpl : public Direct3DRMArrayBase<IDirect3DRMFrame, IDirect3DRMFrameArray> {
	using Direct3DRMArrayBase::Direct3DRMArrayBase;
};

struct Direct3DRMLightArrayImpl : public Direct3DRMArrayBase<IDirect3DRMLight, IDirect3DRMLightArray> {
	using Direct3DRMArrayBase::Direct3DRMArrayBase;
};

struct Direct3DRMViewportArrayImpl : public Direct3DRMArrayBase<IDirect3DRMViewport, IDirect3DRMViewportArray> {
	using Direct3DRMArrayBase::Direct3DRMArrayBase;
};

struct Direct3DRMVisualArrayImpl : public Direct3DRMArrayBase<IDirect3DRMVisual, IDirect3DRMVisualArray> {
	using Direct3DRMArrayBase::Direct3DRMArrayBase;
};

struct PickRecord {
	IDirect3DRMVisual* visual;
	IDirect3DRMFrameArray* frameArray;
	D3DRMPICKDESC desc;
};

struct Direct3DRMPickedArrayImpl : public IDirect3DRMPickedArray {
	~Direct3DRMPickedArrayImpl() override
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
	DWORD GetSize() override { return static_cast<DWORD>(picks.size()); }
	HRESULT GetPick(DWORD index, IDirect3DRMVisual** visual, IDirect3DRMFrameArray** frameArray, D3DRMPICKDESC* desc)
		override
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

private:
	std::vector<PickRecord> picks;
};

struct Direct3DRMWinDeviceImpl : public IDirect3DRMWinDevice {
	HRESULT Activate() override { return DD_OK; }
	HRESULT Paint() override { return DD_OK; }
	void HandleActivate(WORD wParam) override {}
	void HandlePaint(void* p_dc) override {}
};

template <typename T>
struct Direct3DRMObjectBase : public T {
	ULONG Release() override
	{
		if (IUnknown::m_refCount == 1) {
			for (auto it = m_callbacks.cbegin(); it != m_callbacks.cend(); it++) {
				it->first(this, it->second);
			}
		}
		return this->T::Release();
	}
	HRESULT AddDestroyCallback(D3DRMOBJECTCALLBACK callback, void* arg) override
	{
		m_callbacks.push_back(std::make_pair(callback, arg));
		return D3DRM_OK;
	}
	HRESULT DeleteDestroyCallback(D3DRMOBJECTCALLBACK callback, void* arg) override
	{
		for (auto it = m_callbacks.cbegin(); it != m_callbacks.cend(); it++) {
			if (it->first == callback && it->second == arg) {
				m_callbacks.erase(it);
				return D3DRM_OK;
			}
		}
		return D3DRMERR_NOTFOUND;
	}
	HRESULT SetAppData(LPD3DRM_APPDATA appData) override
	{
		m_appData = appData;
		return D3DRM_OK;
	}
	LPVOID GetAppData() override { return m_appData; }
	HRESULT SetName(const char* name) override
	{
		SDL_free(m_name);
		m_name = NULL;
		if (name) {
			m_name = SDL_strdup(name);
		}
		return D3DRM_OK;
	}
	HRESULT GetName(DWORD* size, char* name) override
	{
		if (!size) {
			return DDERR_INVALIDPARAMS;
		}
		const char* s = m_name ? m_name : "";
		size_t l = SDL_strlen(s);
		if (name) {
			SDL_strlcpy(name, s, *size);
		}
		else {
			*size = l + 1;
		}
		return D3DRM_OK;
	}

private:
	std::vector<std::pair<D3DRMOBJECTCALLBACK, void*>> m_callbacks;
	LPD3DRM_APPDATA m_appData = nullptr;
	char* m_name = nullptr;
};

struct Direct3DRMMeshImpl : public Direct3DRMObjectBase<IDirect3DRMMesh> {
	HRESULT Clone(void** ppObject) override
	{
		*ppObject = static_cast<void*>(new Direct3DRMMeshImpl);
		return DD_OK;
	}
	HRESULT Clone(int flags, GUID iid, void** object) override
	{
		if (SDL_memcmp(&iid, &IID_IDirect3DRMMesh, sizeof(GUID)) == 0) {
			*object = static_cast<IDirect3DRMMesh*>(new Direct3DRMMeshImpl);
			return DD_OK;
		}

		return DDERR_GENERIC;
	}
	HRESULT GetBox(D3DRMBOX* box) override { return DD_OK; }
	HRESULT AddGroup(int vertexCount, int faceCount, int vertexPerFace, void* faceBuffer, D3DRMGROUPINDEX* groupIndex)
		override
	{
		return DD_OK;
	}
	HRESULT GetGroup(
		int groupIndex,
		unsigned int* vertexCount,
		unsigned int* faceCount,
		unsigned int* vertexPerFace,
		DWORD* dataSize,
		unsigned int* data
	) override
	{
		return DD_OK;
	}
	HRESULT SetGroupColor(int groupIndex, D3DCOLOR color) override { return DD_OK; }
	HRESULT SetGroupColorRGB(int groupIndex, float r, float g, float b) override { return DD_OK; }
	HRESULT SetGroupMaterial(int groupIndex, IDirect3DRMMaterial* material) override { return DD_OK; }
	HRESULT SetGroupMapping(D3DRMGROUPINDEX groupIndex, D3DRMMAPPING mapping) override { return DD_OK; }
	HRESULT SetGroupQuality(int groupIndex, D3DRMRENDERQUALITY quality) override { return DD_OK; }
	HRESULT SetVertices(int groupIndex, int offset, int count, D3DRMVERTEX* vertices) override { return DD_OK; }
	HRESULT SetGroupTexture(int groupIndex, IDirect3DRMTexture* texture) override
	{
		m_groupTexture = texture;
		return DD_OK;
	}
	HRESULT GetGroupTexture(int groupIndex, LPDIRECT3DRMTEXTURE* texture) override
	{
		if (!m_groupTexture) {
			return DDERR_GENERIC;
		}
		m_groupTexture->AddRef();
		*texture = m_groupTexture;
		return DD_OK;
	}
	D3DRMMAPPING GetGroupMapping(int groupIndex) override { return D3DRMMAP_PERSPCORRECT; }
	D3DRMRENDERQUALITY GetGroupQuality(int groupIndex) override { return D3DRMRENDER_GOURAUD; }
	HRESULT GetGroupColor(D3DRMGROUPINDEX index) override { return DD_OK; }
	HRESULT GetVertices(int groupIndex, int startIndex, int count, D3DRMVERTEX* vertices) override { return DD_OK; }

private:
	IDirect3DRMTexture* m_groupTexture = nullptr;
};

struct Direct3DRMTextureImpl : public Direct3DRMObjectBase<IDirect3DRMTexture2> {
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override
	{
		if (SDL_memcmp(&riid, &IID_IDirect3DRMTexture2, sizeof(GUID)) == 0) {
			this->IUnknown::AddRef();
			*ppvObject = static_cast<IDirect3DRMTexture2*>(this);
			return DD_OK;
		}
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Direct3DRMTextureImpl does not implement guid");
		return E_NOINTERFACE;
	}
	HRESULT Clone(void** ppObject) override
	{
		*ppObject = static_cast<void*>(new Direct3DRMTextureImpl);
		return DD_OK;
	}
	HRESULT Changed(BOOL pixels, BOOL palette) override { return DD_OK; }
};

struct Direct3DRMDevice2Impl : public Direct3DRMObjectBase<IDirect3DRMDevice2> {
	Direct3DRMDevice2Impl()
	{
		m_viewports = new Direct3DRMViewportArrayImpl;
		m_viewports->AddRef();
	}
	~Direct3DRMDevice2Impl() override { m_viewports->Release(); }
	HRESULT Clone(void** ppObject) override
	{
		*ppObject = static_cast<void*>(new Direct3DRMDevice2Impl);
		return DD_OK;
	}
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
	HRESULT AddViewport(IDirect3DRMViewport* viewport) override { return m_viewports->AddElement(viewport); }
	HRESULT GetViewports(IDirect3DRMViewportArray** ppViewportArray) override
	{
		*ppViewportArray = m_viewports;
		return DD_OK;
	}

private:
	IDirect3DRMViewportArray* m_viewports;
};

struct Direct3DRMFrameImpl : public Direct3DRMObjectBase<IDirect3DRMFrame2> {
	Direct3DRMFrameImpl()
	{
		m_children = new Direct3DRMFrameArrayImpl;
		m_children->AddRef();
		m_lights = new Direct3DRMLightArrayImpl;
		m_lights->AddRef();
		m_visuals = new Direct3DRMVisualArrayImpl;
		m_visuals->AddRef();
	}
	~Direct3DRMFrameImpl() override
	{
		m_children->Release();
		m_lights->Release();
		m_visuals->Release();
		if (m_texture) {
			m_texture->Release();
		}
	}
	HRESULT Clone(void** ppObject) override
	{
		*ppObject = static_cast<void*>(new Direct3DRMFrameImpl);
		return DD_OK;
	}
	HRESULT AddChild(IDirect3DRMFrame* child) override { return m_children->AddElement(child); }
	HRESULT DeleteChild(IDirect3DRMFrame* child) override { return m_children->DeleteElement(child); }
	HRESULT SetSceneBackgroundRGB(float r, float g, float b) override { return DD_OK; }
	HRESULT AddLight(IDirect3DRMLight* light) override { return m_lights->AddElement(light); }
	HRESULT GetLights(IDirect3DRMLightArray** lightArray) override
	{
		*lightArray = m_lights;
		m_lights->AddRef();
		return DD_OK;
	}
	HRESULT AddTransform(D3DRMCOMBINETYPE combine, D3DRMMATRIX4D matrix) override { return DD_OK; }
	HRESULT GetPosition(int index, D3DVECTOR* position) override { return DD_OK; }
	HRESULT AddVisual(IDirect3DRMVisual* visual) override { return m_visuals->AddElement(visual); }
	HRESULT DeleteVisual(IDirect3DRMVisual* visual) override { return m_visuals->DeleteElement(visual); }
	HRESULT AddVisual(IDirect3DRMMesh* visual) override { return m_visuals->AddElement(visual); }
	HRESULT DeleteVisual(IDirect3DRMMesh* visual) override { return m_visuals->DeleteElement(visual); }
	HRESULT AddVisual(IDirect3DRMFrame* visual) override { return m_visuals->AddElement(visual); }
	HRESULT DeleteVisual(IDirect3DRMFrame* visual) override { return m_visuals->DeleteElement(visual); }
	HRESULT GetVisuals(IDirect3DRMVisualArray** visuals) override
	{
		*visuals = m_visuals;
		m_visuals->AddRef();
		return DD_OK;
	}
	HRESULT SetTexture(IDirect3DRMTexture* texture) override
	{
		if (m_texture) {
			m_texture->Release();
		}
		m_texture = texture;
		m_texture->AddRef();
		return DD_OK;
	}
	HRESULT GetTexture(IDirect3DRMTexture** texture) override
	{
		if (!m_texture) {
			return DDERR_GENERIC;
		}
		*texture = m_texture;
		m_texture->AddRef();
		return DD_OK;
	}
	HRESULT SetColor(float r, float g, float b, float a) override { return DD_OK; }
	HRESULT SetColor(D3DCOLOR) override { return DD_OK; }
	HRESULT SetColorRGB(float r, float g, float b) override { return DD_OK; }
	HRESULT SetMaterialMode(D3DRMMATERIALMODE mode) override { return DD_OK; }
	HRESULT GetChildren(IDirect3DRMFrameArray** children) override
	{
		*children = m_children;
		m_children->AddRef();
		return DD_OK;
	}

private:
	IDirect3DRMFrameArray* m_children;
	IDirect3DRMLightArray* m_lights;
	IDirect3DRMVisualArray* m_visuals;
	IDirect3DRMTexture* m_texture = nullptr;
};

struct Direct3DRMViewportImpl : public Direct3DRMObjectBase<IDirect3DRMViewport> {
	HRESULT Clone(void** ppObject) override
	{
		*ppObject = static_cast<void*>(new Direct3DRMViewportImpl);
		return DD_OK;
	}
	HRESULT Render(IDirect3DRMFrame* group) override { return DD_OK; }
	HRESULT ForceUpdate(int x, int y, int w, int h) override { return DD_OK; }
	HRESULT Clear() override { return DD_OK; }
	HRESULT SetCamera(IDirect3DRMFrame* camera) override
	{
		m_camera = camera;
		return DD_OK;
	}
	HRESULT GetCamera(IDirect3DRMFrame** camera) override
	{
		*camera = m_camera;
		return DD_OK;
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
	IDirect3DRMFrame* m_camera = nullptr;
};

struct Direct3DRMLightImpl : public Direct3DRMObjectBase<IDirect3DRMLight> {
	HRESULT Clone(void** ppObject) override
	{
		*ppObject = static_cast<void*>(new Direct3DRMLightImpl);
		return DD_OK;
	}
	HRESULT SetColorRGB(float r, float g, float b) override { return DD_OK; }
};

struct Direct3DRMMaterialImpl : public Direct3DRMObjectBase<IDirect3DRMMaterial> {
	HRESULT Clone(void** ppObject) override
	{
		*ppObject = static_cast<void*>(new Direct3DRMMaterialImpl);
		return DD_OK;
	}
};

struct Direct3DRMImpl : virtual public IDirect3DRM2 {
	// IUnknown interface
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override
	{
		if (SDL_memcmp(&riid, &IID_IDirect3DRM2, sizeof(GUID)) == 0) {
			this->IUnknown::AddRef();
			*ppvObject = static_cast<IDirect3DRM2*>(this);
			return DD_OK;
		}
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DirectDrawImpl does not implement guid");
		return E_NOINTERFACE;
	}
	// IDirect3DRM interface
	HRESULT CreateDeviceFromD3D(const IDirect3D2* d3d, IDirect3DDevice2* d3dDevice, IDirect3DRMDevice2** outDevice)
		override
	{
		*outDevice = static_cast<IDirect3DRMDevice2*>(new Direct3DRMDevice2Impl);
		return DD_OK;
	}
	HRESULT CreateDeviceFromSurface(
		const GUID* guid,
		IDirectDraw* dd,
		IDirectDrawSurface* surface,
		IDirect3DRMDevice2** outDevice
	) override
	{
		*outDevice = static_cast<IDirect3DRMDevice2*>(new Direct3DRMDevice2Impl);
		return DD_OK;
	}
	HRESULT CreateTexture(D3DRMIMAGE* image, IDirect3DRMTexture2** outTexture) override
	{
		*outTexture = static_cast<IDirect3DRMTexture2*>(new Direct3DRMTextureImpl);
		return DD_OK;
	}
	HRESULT CreateTextureFromSurface(LPDIRECTDRAWSURFACE surface, IDirect3DRMTexture2** outTexture) override
	{
		*outTexture = static_cast<IDirect3DRMTexture2*>(new Direct3DRMTextureImpl);
		return DD_OK;
	}
	HRESULT CreateMesh(IDirect3DRMMesh** outMesh) override
	{
		*outMesh = static_cast<IDirect3DRMMesh*>(new Direct3DRMMeshImpl);
		return DDERR_GENERIC;
	}
	HRESULT CreateMaterial(D3DVAL power, IDirect3DRMMaterial** outMaterial) override
	{
		*outMaterial = static_cast<IDirect3DRMMaterial*>(new Direct3DRMMaterialImpl);
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
		device->AddViewport(*outViewport);
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
