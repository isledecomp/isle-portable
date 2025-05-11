#pragma once

#include "miniwin_ddraw.h"

// --- Defines and Macros ---
#define D3DPAL_RESERVED 0x00000000
#define D3DPAL_READONLY 0x00000001
#define D3DPTEXTURECAPS_PERSPECTIVE 0x00000001
#define D3DDD_DEVICEZBUFFERBITDEPTH 32

// --- Enums ---
enum D3DCOLORMODEL {
	D3DCOLOR_NONE,
	D3DCOLOR_RGB,
	D3DCOLOR_MONO
};

// --- GUIDs ---
DEFINE_GUID(IID_IDirect3D2, 0x6aae1ec1, 0x662a, 0x11cf, 0x9a, 0x39, 0x00, 0xaa, 0x00, 0x6e, 0xf5, 0x5a);

// --- Structs ---
struct D3DVECTOR {
	float x, y, z;
};

struct D3DDEVICEDESC {
	DWORD dwFlags;
	DWORD dwDeviceZBufferBitDepth;
	D3DCOLORMODEL dcmColorModel;
	DWORD dwDeviceRenderBitDepth;
	struct {
		DWORD dwShadeCaps;
		DWORD dwTextureCaps;
		DWORD dwTextureFilterCaps;
	} dpcTriCaps;
};
typedef D3DDEVICEDESC* LPD3DDEVICEDESC;

struct IDirect3DDevice2 : public IUnknown {};

typedef HRESULT (*LPD3DENUMDEVICESCALLBACK)(LPGUID, LPSTR, LPSTR, LPD3DDEVICEDESC, LPD3DDEVICEDESC, LPVOID);
struct IDirect3D2 {
	virtual ULONG AddRef() { return 0; }
	virtual ULONG Release() { return 0; }
	virtual HRESULT CreateDevice(const GUID& guid, void* pBackBuffer, IDirect3DDevice2** ppDirect3DDevice)
	{
		return DDERR_GENERIC;
	}
	virtual HRESULT EnumDevices(LPD3DENUMDEVICESCALLBACK cb, void* ctx) { return S_OK; };
};
typedef IDirect3D2* LPDIRECT3D2;
