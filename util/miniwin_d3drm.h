#pragma once

#include "miniwin_d3d.h"

// --- Defines and Macros ---
#define D3DRM_OK DD_OK
#define MAXSHORT ((short) 0x7fff)
#define SUCCEEDED(hr) ((hr) >= D3DRM_OK)
#define D3DPTFILTERCAPS_LINEAR 0x00000001
#define D3DPSHADECAPS_ALPHAFLATBLEND 0x00000100

// --- Typedefs ---
typedef DWORD D3DRMMAPPING, *LPD3DRMMAPPING;
typedef float D3DVAL;
typedef void* LPD3DRM_APPDATA;
typedef unsigned long D3DRMGROUPINDEX;
typedef DWORD D3DRMRENDERQUALITY, *LPD3DRMRENDERQUALITY;
typedef DWORD D3DCOLOR, *LPD3DCOLOR;
typedef float D3DVALUE, *LPD3DVALUE;

// --- Enums ---
enum D3DRMCOMBINETYPE {
	D3DRMCOMBINE_REPLACE,
	D3DRMCOMBINE_BEFORE,
	D3DRMCOMBINE_AFTER
};
typedef D3DRMCOMBINETYPE* LPD3DRMCOMBINETYPE;

enum D3DRMPALETTEFLAGS {
	D3DRMPALETTE_READONLY
};
typedef D3DRMPALETTEFLAGS* LPD3DRMPALETTEFLAGS;

enum D3DRMTEXTUREQUALITY {
	D3DRMTEXTURE_NEAREST,
	D3DRMTEXTURE_LINEAR,
	D3DRMTEXTURE_MIPNEAREST,
	D3DRMTEXTURE_MIPLINEAR,
	D3DRMTEXTURE_LINEARMIPNEAREST,
	D3DRMTEXTURE_LINEARMIPLINEAR
};
typedef D3DRMTEXTUREQUALITY* LPD3DRMTEXTUREQUALITY;

enum D3DRMRENDERMODE {
	D3DRMRENDERMODE_BLENDEDTRANSPARENCY
};

enum D3DRMMappingFlag {
	D3DRMMAP_WRAPU = 1,
	D3DRMMAP_WRAPV = 2,
	D3DRMMAP_PERSPCORRECT = 4
};

enum D3DRMLIGHTTYPE {
	D3DRMLIGHT_AMBIENT,
	D3DRMLIGHT_POINT,
	D3DRMLIGHT_SPOT,
	D3DRMLIGHT_DIRECTIONAL,
	D3DRMLIGHT_PARALLELPOINT
};
typedef D3DRMLIGHTTYPE* LPD3DRMLIGHTTYPE;

enum D3DRMMATERIALMODE {
	D3DRMMATERIAL_FROMPARENT,
	D3DRMMATERIAL_FROMFRAME,
	D3DRMMATERIAL_FROMMESH
};

enum D3DRMRenderMode {
	D3DRMRENDER_WIREFRAME = 0,
	D3DRMRENDER_UNLITFLAT = 1,
	D3DRMRENDER_FLAT = 2,
	D3DRMRENDER_GOURAUD = 4,
	D3DRMRENDER_PHONG = 8
};

enum D3DRMPROJECTIONTYPE {
	D3DRMPROJECT_PERSPECTIVE,
	D3DRMPROJECT_ORTHOGRAPHIC
};
typedef D3DRMPROJECTIONTYPE* LPD3DRMPROJECTIONTYPE;

// --- GUIDs ---
DEFINE_GUID(IID_IDirect3DRM2, 0x4516ec41, 0x8f20, 0x11d0, 0x9b, 0x6d, 0x00, 0x00, 0xc0, 0x78, 0x1b, 0xc3);
DEFINE_GUID(IID_IDirect3DRMWinDevice, 0x0eb16e60, 0xcbf4, 0x11cf, 0xa5, 0x3c, 0x00, 0x20, 0xaf, 0x70, 0x7e, 0xfd);
DEFINE_GUID(IID_IDirect3DRMMesh, 0x4516ec78, 0x8f20, 0x11d0, 0x9b, 0x6d, 0x00, 0x00, 0xc0, 0x78, 0x1b, 0xc3);
DEFINE_GUID(IID_IDirect3DRMMeshBuilder, 0x4516ec7b, 0x8f20, 0x11d0, 0x9b, 0x6d, 0x00, 0x00, 0xc0, 0x78, 0x1b, 0xc3);
DEFINE_GUID(IID_IDirect3DRMTexture2, 0x120f30c0, 0x1629, 0x11d1, 0x94, 0x26, 0x00, 0x60, 0x97, 0x0c, 0xf4, 0x0d);

// --- Structs ---
typedef struct D3DRMVECTOR4D {
	float x, y, z, w;
} D3DRMVECTOR4D;

typedef struct D3DRMPALETTEENTRY {
	unsigned char red, green, blue, flags;
} D3DRMPALETTEENTRY;

typedef struct D3DRMIMAGE {
	unsigned int width, height, depth, bytes_per_line;
	unsigned int red_mask, green_mask, blue_mask, alpha_mask;
	unsigned int palette_size;
	D3DRMPALETTEENTRY* palette;
	void* buffer1;
	void* buffer2;
	void* data;
	int rgb;
	int aspectx, aspecty;
	unsigned int format;
} D3DRMIMAGE;

typedef struct D3DRMMATRIX4D {
	double* operator[](size_t i) { return 0; }
	const double* operator[](size_t i) const { return 0; }
} D3DRMMATRIX4D;

struct D3DRMBOX {
	D3DVECTOR min;
	D3DVECTOR max;
};

struct D3DRMVERTEX {
	D3DVECTOR position;
	D3DVECTOR normal;
	float tu, tv;
};

struct IDirect3DRMVisual : public IUnknown {};
typedef IDirect3DRMVisual* LPDIRECT3DRMVISUAL;

struct IDirect3DRMObject : public IUnknown {
	virtual HRESULT Clone(void** ppObject) { return DDERR_GENERIC; }
	virtual HRESULT AddDestroyCallback(void (*cb)(IDirect3DRMObject*, void*), void* arg) { return D3DRM_OK; }
	virtual HRESULT DeleteDestroyCallback(void (*cb)(IDirect3DRMObject*, void*), void* arg) { return D3DRM_OK; }
	virtual HRESULT SetAppData(LPD3DRM_APPDATA appData) { return D3DRM_OK; }
	virtual DWORD GetAppData() = 0;
	virtual HRESULT SetName(const char* name) { return D3DRM_OK; }
	virtual HRESULT GetName(DWORD* size, char* name) { return D3DRM_OK; }
	virtual HRESULT GetClassName(DWORD* size, char* name) { return D3DRM_OK; }
};
struct IDirect3DRMTexture : public IUnknown {
	virtual HRESULT AddDestroyCallback(void (*cb)(IDirect3DRMObject*, void*), void* arg) { return D3DRM_OK; }
	virtual DWORD GetAppData() { return 0; }
	virtual HRESULT SetAppData(LPD3DRM_APPDATA appData) { return D3DRM_OK; }
	virtual HRESULT SetTexture(const IDirect3DRMTexture* texture) { return D3DRM_OK; }
	virtual HRESULT Changed(BOOL arg1, BOOL arg2) { return D3DRM_OK; }
};
typedef IDirect3DRMTexture* LPDIRECT3DRMTEXTURE;

struct IDirect3DRMTexture2 : public IDirect3DRMTexture {};
typedef IDirect3DRMTexture2* LPDIRECT3DRMTEXTURE2;

typedef struct IDirect3DRMMaterial *LPDIRECT3DRMMATERIAL, **LPLPDIRECT3DRMMATERIAL;

struct IDirect3DRMMesh {
	virtual ULONG Release() { return 0; }
	virtual HRESULT Clone(int flags, GUID iid, void** object) { return DDERR_GENERIC; }
	virtual HRESULT GetBox(D3DRMBOX* box) { return D3DRM_OK; }
	virtual HRESULT AddGroup(
		int vertexCount,
		int faceCount,
		int vertexPerFace,
		void* faceBuffer,
		D3DRMGROUPINDEX* groupIndex
	)
	{
		return D3DRM_OK;
	}
	virtual HRESULT GetGroup(
		int groupIndex,
		unsigned int* vertexCount,
		unsigned int* faceCount,
		unsigned int* vertexPerFace,
		DWORD* dataSize,
		unsigned int* data
	)
	{
		return DDERR_GENERIC;
	}
	virtual HRESULT SetGroupColor(int groupIndex, D3DCOLOR color) { return D3DRM_OK; }
	virtual HRESULT SetGroupColorRGB(int groupIndex, float r, float g, float b) { return D3DRM_OK; }
	virtual HRESULT SetGroupTexture(int groupIndex, const IDirect3DRMTexture* texture) { return D3DRM_OK; }
	virtual HRESULT SetGroupMaterial(int groupIndex, IDirect3DRMMaterial* material) { return D3DRM_OK; }
	virtual HRESULT SetGroupMapping(int groupIndex, int mapping) { return D3DRM_OK; }
	virtual HRESULT SetGroupQuality(int groupIndex, D3DRMRENDERQUALITY quality) { return D3DRM_OK; }
	virtual HRESULT SetVertices(int groupIndex, int offset, int count, D3DRMVERTEX* vertices) { return D3DRM_OK; }
	virtual HRESULT GetGroupTexture(int groupIndex, LPDIRECT3DRMTEXTURE* texture) { return DDERR_GENERIC; }
	virtual HRESULT GetGroupMapping(int groupIndex) { return D3DRM_OK; }
	virtual HRESULT GetGroupQuality(int groupIndex) { return D3DRM_OK; }
	virtual HRESULT GetGroupColor(D3DRMGROUPINDEX index) { return D3DRM_OK; }
	virtual HRESULT GetVertices(int groupIndex, int startIndex, int count, D3DRMVERTEX* vertices) { return D3DRM_OK; }
};

struct IDirect3DRMLight {
	virtual ULONG AddRef() { return 0; }
	virtual ULONG Release() { return 0; }
	virtual HRESULT SetColorRGB(float r, float g, float b) { return D3DRM_OK; }
};

struct IDirect3DRMLightArray {
	virtual ULONG Release() { return 0; }
	virtual DWORD GetSize() { return 0; }
	virtual HRESULT GetElement(int index, IDirect3DRMLight** light) const { return DDERR_GENERIC; }
};

struct IDirect3DRMVisualArray {
	virtual ULONG Release() { return 0; }
	virtual DWORD GetSize() { return 0; }
	virtual HRESULT GetElement(int index, IDirect3DRMVisual** visual) const { return DDERR_GENERIC; }
};

typedef struct IDirect3DRMFrameArray* LPDIRECT3DRMFRAMEARRAY;
struct IDirect3DRMFrame {
	virtual ULONG AddRef() { return 0; }
	virtual ULONG Release() { return 0; }
	virtual HRESULT SetAppData(LPD3DRM_APPDATA appData) { return D3DRM_OK; }
	virtual DWORD GetAppData() { return 0; }
	virtual HRESULT AddChild(IDirect3DRMFrame* child) { return D3DRM_OK; }
	virtual HRESULT DeleteChild(IDirect3DRMFrame* child) { return D3DRM_OK; }
	virtual HRESULT SetSceneBackgroundRGB(float r, float g, float b) { return D3DRM_OK; }
	virtual HRESULT AddLight(IDirect3DRMLight* light) { return D3DRM_OK; }
	virtual HRESULT GetLights(IDirect3DRMLightArray** lightArray) { return DDERR_GENERIC; }
	virtual HRESULT AddTransform(int combine, D3DRMMATRIX4D matrix) { return D3DRM_OK; }
	virtual HRESULT GetPosition(int index, D3DVECTOR* position) { return D3DRM_OK; }
	virtual HRESULT AddVisual(IDirect3DRMVisual* visual) { return D3DRM_OK; }
	virtual HRESULT DeleteVisual(IDirect3DRMVisual* visual) { return D3DRM_OK; }
	virtual HRESULT AddVisual(IDirect3DRMMesh* visual) { return D3DRM_OK; }
	virtual HRESULT DeleteVisual(IDirect3DRMMesh* visual) { return D3DRM_OK; }
	virtual HRESULT AddVisual(IDirect3DRMFrame* visual) { return D3DRM_OK; }
	virtual HRESULT DeleteVisual(IDirect3DRMFrame* visual) { return D3DRM_OK; }
	virtual HRESULT GetVisuals(IDirect3DRMVisualArray** visuals) { return DDERR_GENERIC; }
	virtual HRESULT SetTexture(const IDirect3DRMTexture* texture) { return D3DRM_OK; }
	virtual HRESULT GetTexture(IDirect3DRMTexture** texture) { return DDERR_GENERIC; }
	virtual HRESULT SetColor(float r, float g, float b, float a) { return D3DRM_OK; }
	virtual HRESULT SetColor(D3DCOLOR) { return D3DRM_OK; }
	virtual HRESULT SetColorRGB(float r, float g, float b) { return D3DRM_OK; }
	virtual HRESULT SetMaterialMode(D3DRMMATERIALMODE mode) { return D3DRM_OK; }
	virtual HRESULT GetChildren(IDirect3DRMFrameArray** children) { return DDERR_GENERIC; }
};
typedef IDirect3DRMFrame* LPDIRECT3DRMFRAME;

struct IDirect3DRMFrame2 : public IDirect3DRMFrame {};
typedef IDirect3DRMFrame2* LPDIRECT3DRMFRAME2;

struct IDirect3DRMFrameArray : public IUnknown {
	virtual DWORD GetSize() { return 0; }
	virtual HRESULT GetElement(DWORD index, IDirect3DRMFrame** frame) { return DDERR_GENERIC; }
};

struct D3DRMPICKDESC {
	IDirect3DRMVisual* visual;
	IDirect3DRMFrame* frame;
	float dist;
};

struct IDirect3DRMPickedArray {
	virtual ULONG Release() { return 0; }
	DWORD GetSize() { return 0; }
	HRESULT GetPick(DWORD index, IDirect3DRMVisual** visual, IDirect3DRMFrameArray** frameArray, D3DRMPICKDESC* desc)
	{
		return DDERR_GENERIC;
	}
};
typedef IDirect3DRMPickedArray* LPDIRECT3DRMPICKEDARRAY;

struct IDirect3DRMViewport : public IDirect3DRMObject {
	virtual HRESULT Render(IDirect3DRMFrame* group) { return D3DRM_OK; }
	virtual HRESULT ForceUpdate(int x, int y, int w, int h) { return D3DRM_OK; }
	virtual HRESULT Clear() { return D3DRM_OK; }
	virtual HRESULT SetCamera(IDirect3DRMFrame* camera) { return D3DRM_OK; }
	virtual HRESULT GetCamera(IDirect3DRMFrame** camera) { return DDERR_GENERIC; }
	virtual HRESULT SetProjection(D3DRMPROJECTIONTYPE type) { return D3DRM_OK; }
	virtual D3DRMPROJECTIONTYPE GetProjection() { return D3DRMPROJECT_PERSPECTIVE; }
	virtual HRESULT SetFront(D3DVALUE z) { return D3DRM_OK; }
	virtual D3DVALUE GetFront() { return 1.0f; }
	virtual HRESULT SetBack(D3DVALUE z) { return D3DRM_OK; }
	virtual D3DVALUE GetBack() { return 100.0f; }
	virtual HRESULT SetField(D3DVALUE field) { return D3DRM_OK; }
	virtual D3DVALUE GetField() { return 1.0f; }
	virtual int GetWidth() { return 640; }
	virtual int GetHeight() { return 480; }
	virtual HRESULT Transform(D3DRMVECTOR4D* screen, D3DVECTOR* world) { return D3DRM_OK; }
	virtual HRESULT InverseTransform(D3DVECTOR* world, D3DRMVECTOR4D* screen) { return D3DRM_OK; }
	virtual HRESULT Pick(float x, float y, LPDIRECT3DRMPICKEDARRAY* pickedArray) { return D3DRM_OK; }
};

struct IDirect3DRMWinDevice : public IUnknown {
	virtual HRESULT Activate() { return D3DRM_OK; }
	virtual HRESULT Paint() { return D3DRM_OK; }
	virtual void HandleActivate(WORD wParam) {}
	virtual void HandlePaint(void* p_dc) {}
};

struct IDirect3DRMViewportArray {
	virtual ULONG AddRef() { return 0; }
	virtual ULONG Release() { return 0; }
	virtual DWORD GetSize() { return 0; }
	virtual HRESULT GetElement(int index, IDirect3DRMViewport** viewport) { return DDERR_GENERIC; }
};

struct IDirect3DRMDevice2 : public IUnknown {
	virtual unsigned long GetWidth() { return 640; }
	virtual unsigned long GetHeight() { return 480; }
	virtual HRESULT SetBufferCount(int count) { return D3DRM_OK; }
	virtual HRESULT GetBufferCount() { return D3DRM_OK; }
	virtual HRESULT SetShades(unsigned long shadeCount) { return D3DRM_OK; }
	virtual HRESULT GetShades() { return D3DRM_OK; }
	virtual HRESULT SetQuality(int quality) { return D3DRM_OK; }
	virtual HRESULT GetQuality() { return D3DRM_OK; }
	virtual HRESULT SetDither(int dither) { return D3DRM_OK; }
	virtual HRESULT GetDither() { return D3DRM_OK; }
	virtual HRESULT SetTextureQuality(D3DRMTEXTUREQUALITY quality) { return D3DRM_OK; }
	virtual D3DRMTEXTUREQUALITY GetTextureQuality() { return D3DRMTEXTURE_NEAREST; }
	virtual HRESULT SetRenderMode(int mode) { return D3DRM_OK; }
	virtual HRESULT GetRenderMode() { return D3DRM_OK; }
	virtual HRESULT Update() { return D3DRM_OK; }
	virtual HRESULT GetViewports(IDirect3DRMViewportArray** ppViewportArray) { return DDERR_GENERIC; }
};

struct IDirect3DRM : public IUnknown {
	virtual HRESULT CreateDeviceFromD3D(
		const IDirect3D2* d3d,
		IDirect3DDevice2* d3dDevice,
		IDirect3DRMDevice2** outDevice
	)
	{
		return DDERR_GENERIC;
	}
	virtual HRESULT CreateDeviceFromSurface(
		const GUID* guid,
		IDirectDraw* dd,
		IDirectDrawSurface* surface,
		IDirect3DRMDevice2** outDevice
	)
	{
		return DDERR_GENERIC;
	}
	virtual HRESULT CreateTexture(D3DRMIMAGE* image, IDirect3DRMTexture2** outTexture) { return DDERR_GENERIC; }
	virtual HRESULT CreateTextureFromSurface(LPDIRECTDRAWSURFACE surface, IDirect3DRMTexture2** outTexture)
	{
		return DDERR_GENERIC;
	}
	virtual HRESULT CreateMesh(IDirect3DRMMesh** outMesh) { return DDERR_GENERIC; }
	virtual HRESULT CreateMaterial(D3DVAL power, IDirect3DRMMaterial** outMaterial) { return DDERR_GENERIC; }
	virtual HRESULT CreateLightRGB(D3DRMLIGHTTYPE type, D3DVAL r, D3DVAL g, D3DVAL b, IDirect3DRMLight** outLight)
	{
		return DDERR_GENERIC;
	}
	virtual HRESULT CreateFrame(IDirect3DRMFrame* parent, IDirect3DRMFrame2** outFrame) { return DDERR_GENERIC; }
	virtual HRESULT CreateViewport(
		IDirect3DRMDevice2* device,
		IDirect3DRMFrame* camera,
		int x,
		int y,
		int width,
		int height,
		IDirect3DRMViewport** outViewport
	)
	{
		return DDERR_GENERIC;
	}
	virtual HRESULT SetDefaultTextureShades(unsigned int count) { return D3DRM_OK; }
	virtual HRESULT SetDefaultTextureColors(unsigned int count) { return D3DRM_OK; }
};
typedef IDirect3DRM *LPDIRECT3DRM, **LPLPDIRECT3DRM;

struct IDirect3DRM2 : public IDirect3DRM {};

// Functions
inline HRESULT WINAPI Direct3DRMCreate(IDirect3DRM** direct3DRM)
{
	return DDERR_GENERIC;
}

inline D3DCOLOR D3DRMCreateColorRGBA(D3DVALUE red, D3DVALUE green, D3DVALUE blue, D3DVALUE alpha)
{
	return 0;
}
