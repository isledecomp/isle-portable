#pragma once

#include "miniwin_ddraw.h"

// --- Defines and Macros ---
#define D3DPAL_RESERVED 0x80
#define D3DPAL_READONLY 0x40

#define DDBD_8 0x00000800
#define DDBD_16 0x00000400
#define DDBD_24 0x00000200
#define DDBD_32 0x00000100

#define D3DDD_DEVICEZBUFFERBITDEPTH 0x00000100 // dwDeviceZBufferBitDepth is valid

#define D3DPSHADECAPS_ALPHAFLATBLEND 0x00001000 // Supports D3DRMRENDERMODE_BLENDEDTRANSPARENCY

#define D3DPTEXTURECAPS_PERSPECTIVE 0x00000001 // Supports perspective correction texturing

#define D3DPTFILTERCAPS_LINEAR 0x00000002 // Supports bilinear filtering

#define D3DCOLOR_NONE D3DCOLORMODEL::NONE
#define D3DCOLOR_RGB D3DCOLORMODEL::RGB
#define D3DCOLOR_MONO D3DCOLORMODEL::MONO

// --- Enums ---
enum class D3DCOLORMODEL {
	NONE = 0,
	RGB = 1,
	MONO = 2
};

// --- GUIDs ---
DEFINE_GUID(IID_IDirect3D2, 0x6aae1ec1, 0x662a, 0x11d0, 0x88, 0x9d, 0x00, 0xaa, 0x00, 0xbb, 0xb7, 0x6a);

// --- Structs ---
struct D3DVECTOR {
	float x, y, z;
};

struct D3DDEVICEDESC {
	DWORD dwFlags;                 // D3DDD_*
	DWORD dwDeviceZBufferBitDepth; // DDBD_*
	D3DCOLORMODEL dcmColorModel;   // D3DCOLOR_* Bit flag, but Isle think it's an enum
	DWORD dwDeviceRenderBitDepth;  // DDBD_*
	struct {
		DWORD dwShadeCaps;         // D3DPSHADECAPS_*
		DWORD dwTextureCaps;       // D3DPTEXTURECAPS_*
		DWORD dwTextureFilterCaps; // D3DPTFILTERCAPS_*
	} dpcTriCaps;
};
typedef D3DDEVICEDESC* LPD3DDEVICEDESC;

struct IDirect3DDevice2 : virtual public IUnknown {};

typedef HRESULT (*LPD3DENUMDEVICESCALLBACK)(LPGUID, LPSTR, LPSTR, LPD3DDEVICEDESC, LPD3DDEVICEDESC, LPVOID);
struct IDirect3D2 : virtual public IUnknown {
	virtual HRESULT CreateDevice(const GUID& guid, void* pBackBuffer, IDirect3DDevice2** ppDirect3DDevice) = 0;
	virtual HRESULT EnumDevices(LPD3DENUMDEVICESCALLBACK cb, void* ctx) = 0;
};
typedef IDirect3D2* LPDIRECT3D2;
