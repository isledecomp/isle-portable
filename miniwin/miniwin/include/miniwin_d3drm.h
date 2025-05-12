#pragma once

#include "miniwin_d3d.h"

#include <stdlib.h> // abort // FIXME: remove

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
	double* operator[](size_t i) { abort(); }
	const double* operator[](size_t i) const { abort(); }
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
	virtual HRESULT Clone(void** ppObject) = 0;
	virtual HRESULT AddDestroyCallback(void (*cb)(IDirect3DRMObject*, void*), void* arg) = 0;
	virtual HRESULT DeleteDestroyCallback(void (*cb)(IDirect3DRMObject*, void*), void* arg) = 0;
	virtual HRESULT SetAppData(LPD3DRM_APPDATA appData) = 0;
	virtual LPVOID GetAppData() = 0;
	virtual HRESULT SetName(const char* name) = 0;
	virtual HRESULT GetName(DWORD* size, char* name) = 0;
	virtual HRESULT GetClassName(DWORD* size, char* name) = 0;
};
struct IDirect3DRMTexture : public IUnknown {
	virtual HRESULT AddDestroyCallback(void (*cb)(IDirect3DRMObject*, void*), void* arg) = 0;
	virtual LPVOID GetAppData() = 0;
	virtual HRESULT SetAppData(LPD3DRM_APPDATA appData) = 0;
	virtual HRESULT SetTexture(const IDirect3DRMTexture* texture) = 0;
	virtual HRESULT Changed(BOOL arg1, BOOL arg2) = 0;
};
typedef IDirect3DRMTexture* LPDIRECT3DRMTEXTURE;

struct IDirect3DRMTexture2 : public IDirect3DRMTexture {};
typedef IDirect3DRMTexture2* LPDIRECT3DRMTEXTURE2;

typedef struct IDirect3DRMMaterial *LPDIRECT3DRMMATERIAL, **LPLPDIRECT3DRMMATERIAL;

struct IDirect3DRMMesh : public IUnknown {
	virtual HRESULT Clone(int flags, GUID iid, void** object) = 0;
	virtual HRESULT GetBox(D3DRMBOX* box) = 0;
	virtual HRESULT AddGroup(
		int vertexCount,
		int faceCount,
		int vertexPerFace,
		void* faceBuffer,
		D3DRMGROUPINDEX* groupIndex
	) = 0;
	virtual HRESULT GetGroup(
		int groupIndex,
		unsigned int* vertexCount,
		unsigned int* faceCount,
		unsigned int* vertexPerFace,
		DWORD* dataSize,
		unsigned int* data
	) = 0;
	virtual HRESULT SetGroupColor(int groupIndex, D3DCOLOR color) = 0;
	virtual HRESULT SetGroupColorRGB(int groupIndex, float r, float g, float b) = 0;
	virtual HRESULT SetGroupTexture(int groupIndex, const IDirect3DRMTexture* texture) = 0;
	virtual HRESULT SetGroupMaterial(int groupIndex, IDirect3DRMMaterial* material) = 0;
	virtual HRESULT SetGroupMapping(int groupIndex, int mapping) = 0;
	virtual HRESULT SetGroupQuality(int groupIndex, D3DRMRENDERQUALITY quality) = 0;
	virtual HRESULT SetVertices(int groupIndex, int offset, int count, D3DRMVERTEX* vertices) = 0;
	virtual HRESULT GetGroupTexture(int groupIndex, LPDIRECT3DRMTEXTURE* texture) = 0;
	virtual HRESULT GetGroupMapping(int groupIndex) = 0;
	virtual HRESULT GetGroupQuality(int groupIndex) = 0;
	virtual HRESULT GetGroupColor(D3DRMGROUPINDEX index) = 0;
	virtual HRESULT GetVertices(int groupIndex, int startIndex, int count, D3DRMVERTEX* vertices) = 0;
};

struct IDirect3DRMLight : public IUnknown {
	virtual HRESULT SetColorRGB(float r, float g, float b) = 0;
};

struct IDirect3DRMLightArray : public IUnknown {
	virtual DWORD GetSize() = 0;
	virtual HRESULT GetElement(int index, IDirect3DRMLight** light) const = 0;
};

struct IDirect3DRMVisualArray : public IUnknown {
	virtual DWORD GetSize() = 0;
	virtual HRESULT GetElement(int index, IDirect3DRMVisual** visual) const = 0;
};

typedef struct IDirect3DRMFrameArray* LPDIRECT3DRMFRAMEARRAY;
struct IDirect3DRMFrame : public IUnknown {
	virtual HRESULT SetAppData(LPD3DRM_APPDATA appData) = 0;
	virtual LPVOID GetAppData() = 0;
	virtual HRESULT AddChild(IDirect3DRMFrame* child) = 0;
	virtual HRESULT DeleteChild(IDirect3DRMFrame* child) = 0;
	virtual HRESULT SetSceneBackgroundRGB(float r, float g, float b) = 0;
	virtual HRESULT AddLight(IDirect3DRMLight* light) = 0;
	virtual HRESULT GetLights(IDirect3DRMLightArray** lightArray) = 0;
	virtual HRESULT AddTransform(int combine, D3DRMMATRIX4D matrix) = 0;
	virtual HRESULT GetPosition(int index, D3DVECTOR* position) = 0;
	virtual HRESULT AddVisual(IDirect3DRMVisual* visual) = 0;
	virtual HRESULT DeleteVisual(IDirect3DRMVisual* visual) = 0;
	virtual HRESULT AddVisual(IDirect3DRMMesh* visual) = 0;
	virtual HRESULT DeleteVisual(IDirect3DRMMesh* visual) = 0;
	virtual HRESULT AddVisual(IDirect3DRMFrame* visual) = 0;
	virtual HRESULT DeleteVisual(IDirect3DRMFrame* visual) = 0;
	virtual HRESULT GetVisuals(IDirect3DRMVisualArray** visuals) = 0;
	virtual HRESULT SetTexture(const IDirect3DRMTexture* texture) = 0;
	virtual HRESULT GetTexture(IDirect3DRMTexture** texture) = 0;
	virtual HRESULT SetColor(float r, float g, float b, float a) = 0;
	virtual HRESULT SetColor(D3DCOLOR) = 0;
	virtual HRESULT SetColorRGB(float r, float g, float b) = 0;
	virtual HRESULT SetMaterialMode(D3DRMMATERIALMODE mode) = 0;
	virtual HRESULT GetChildren(IDirect3DRMFrameArray** children) = 0;
};
typedef IDirect3DRMFrame* LPDIRECT3DRMFRAME;

struct IDirect3DRMFrame2 : public IDirect3DRMFrame {};
typedef IDirect3DRMFrame2* LPDIRECT3DRMFRAME2;

struct IDirect3DRMFrameArray : public IUnknown {
	virtual DWORD GetSize() = 0;
	virtual HRESULT GetElement(DWORD index, IDirect3DRMFrame** frame) = 0;
};

struct D3DRMPICKDESC {
	IDirect3DRMVisual* visual;
	IDirect3DRMFrame* frame;
	float dist;
};

struct IDirect3DRMPickedArray : public IUnknown {
	virtual DWORD GetSize() = 0;
	virtual HRESULT GetPick(DWORD index, IDirect3DRMVisual** visual, IDirect3DRMFrameArray** frameArray, D3DRMPICKDESC* desc) = 0;
};
typedef IDirect3DRMPickedArray* LPDIRECT3DRMPICKEDARRAY;

struct IDirect3DRMViewport : public IDirect3DRMObject {
	virtual HRESULT Render(IDirect3DRMFrame* group) = 0;
	virtual HRESULT ForceUpdate(int x, int y, int w, int h) = 0;
	virtual HRESULT Clear() = 0;
	virtual HRESULT SetCamera(IDirect3DRMFrame* camera) = 0;
	virtual HRESULT GetCamera(IDirect3DRMFrame** camera) = 0;
	virtual HRESULT SetProjection(D3DRMPROJECTIONTYPE type) = 0;
	virtual D3DRMPROJECTIONTYPE GetProjection() = 0;
	virtual HRESULT SetFront(D3DVALUE z) = 0;
	virtual D3DVALUE GetFront() = 0;
	virtual HRESULT SetBack(D3DVALUE z) = 0;
	virtual D3DVALUE GetBack() = 0;
	virtual HRESULT SetField(D3DVALUE field) = 0;
	virtual D3DVALUE GetField() = 0;
	virtual int GetWidth() = 0;
	virtual int GetHeight() = 0;
	virtual HRESULT Transform(D3DRMVECTOR4D* screen, D3DVECTOR* world) = 0;
	virtual HRESULT InverseTransform(D3DVECTOR* world, D3DRMVECTOR4D* screen) = 0;
	virtual HRESULT Pick(float x, float y, LPDIRECT3DRMPICKEDARRAY* pickedArray) = 0;
};

struct IDirect3DRMWinDevice : public IUnknown {
	virtual HRESULT Activate() = 0;
	virtual HRESULT Paint() = 0;
	virtual void HandleActivate(WORD wParam) = 0;
	virtual void HandlePaint(void* p_dc) = 0;
};

struct IDirect3DRMViewportArray : public IUnknown {
	virtual DWORD GetSize() = 0;
	virtual HRESULT GetElement(int index, IDirect3DRMViewport** viewport) = 0;
};

struct IDirect3DRMDevice2 : public IUnknown {
	virtual unsigned long GetWidth() = 0;
	virtual unsigned long GetHeight() = 0;
	virtual HRESULT SetBufferCount(int count) = 0;
	virtual HRESULT GetBufferCount() = 0;
	virtual HRESULT SetShades(unsigned long shadeCount) = 0;
	virtual HRESULT GetShades() = 0;
	virtual HRESULT SetQuality(int quality) = 0;
	virtual HRESULT GetQuality() = 0;
	virtual HRESULT SetDither(int dither) = 0;
	virtual HRESULT GetDither() = 0;
	virtual HRESULT SetTextureQuality(D3DRMTEXTUREQUALITY quality) = 0;
	virtual D3DRMTEXTUREQUALITY GetTextureQuality() = 0;
	virtual HRESULT SetRenderMode(int mode) = 0;
	virtual HRESULT GetRenderMode() = 0;
	virtual HRESULT Update() = 0;
	virtual HRESULT GetViewports(IDirect3DRMViewportArray** ppViewportArray) = 0;
};

struct IDirect3DRM : public IUnknown {
	virtual HRESULT CreateDeviceFromD3D(
		const IDirect3D2* d3d,
		IDirect3DDevice2* d3dDevice,
		IDirect3DRMDevice2** outDevice
	) = 0;
	virtual HRESULT CreateDeviceFromSurface(
		const GUID* guid,
		IDirectDraw* dd,
		IDirectDrawSurface* surface,
		IDirect3DRMDevice2** outDevice
	) = 0;
	virtual HRESULT CreateTexture(D3DRMIMAGE* image, IDirect3DRMTexture2** outTexture) = 0;
	virtual HRESULT CreateTextureFromSurface(LPDIRECTDRAWSURFACE surface, IDirect3DRMTexture2** outTexture) = 0;
	virtual HRESULT CreateMesh(IDirect3DRMMesh** outMesh) = 0;
	virtual HRESULT CreateMaterial(D3DVAL power, IDirect3DRMMaterial** outMaterial) = 0;
	virtual HRESULT CreateLightRGB(D3DRMLIGHTTYPE type, D3DVAL r, D3DVAL g, D3DVAL b, IDirect3DRMLight** outLight) = 0;
	virtual HRESULT CreateFrame(IDirect3DRMFrame* parent, IDirect3DRMFrame2** outFrame) = 0;
	virtual HRESULT CreateViewport(
		IDirect3DRMDevice2* device,
		IDirect3DRMFrame* camera,
		int x,
		int y,
		int width,
		int height,
		IDirect3DRMViewport** outViewport
	) = 0;
	virtual HRESULT SetDefaultTextureShades(unsigned int count) = 0;
	virtual HRESULT SetDefaultTextureColors(unsigned int count) = 0;
};
typedef IDirect3DRM *LPDIRECT3DRM, **LPLPDIRECT3DRM;

struct IDirect3DRM2 : public IDirect3DRM {};

// Functions
HRESULT WINAPI Direct3DRMCreate(IDirect3DRM** direct3DRM);

D3DCOLOR D3DRMCreateColorRGBA(D3DVALUE red, D3DVALUE green, D3DVALUE blue, D3DVALUE alpha);
