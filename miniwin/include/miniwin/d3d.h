#pragma once

#include "miniwin/ddraw.h"

// --- GUIDs ---
DEFINE_GUID(IID_IDirect3D2, 0x6aae1ec1, 0x662a, 0x11d0, 0x88, 0x9d, 0x00, 0xaa, 0x00, 0xbb, 0xb7, 0x6a);

// --- Enums ---
#define DDBD_8 DDBitDepths::BPP_8
#define DDBD_16 DDBitDepths::BPP_16
#define DDBD_24 DDBitDepths::BPP_24
#define DDBD_32 DDBitDepths::BPP_32
enum class DDBitDepths : uint32_t {
	BPP_8 = 1 << 11,
	BPP_16 = 1 << 10,
	BPP_24 = 1 << 9,
	BPP_32 = 1 << 8,
};
ENABLE_BITMASK_OPERATORS(DDBitDepths)

#define D3DDD_DEVICEZBUFFERBITDEPTH D3DDeviceDescFlags::DEVICEZBUFFERBITDEPTH
enum class D3DDeviceDescFlags : uint32_t {
	DEVICEZBUFFERBITDEPTH = 1 << 8,
};
ENABLE_BITMASK_OPERATORS(D3DDeviceDescFlags)

#define D3DPSHADECAPS_ALPHAFLATBLEND D3DPShadeCaps::ALPHAFLATBLEND
enum class D3DPShadeCaps : uint32_t {
	ALPHAFLATBLEND = 1 << 12,
};
ENABLE_BITMASK_OPERATORS(D3DPShadeCaps)

#define D3DPTEXTURECAPS_PERSPECTIVE D3DPTextureCaps::PERSPECTIVE
enum class D3DPTextureCaps : uint32_t {
	PERSPECTIVE = 1 << 0,
};
ENABLE_BITMASK_OPERATORS(D3DPTextureCaps)

#define D3DPTFILTERCAPS_LINEAR D3DPTextureFilterCaps::LINEAR
enum class D3DPTextureFilterCaps : uint32_t {
	LINEAR = 1 << 1,
};
ENABLE_BITMASK_OPERATORS(D3DPTextureFilterCaps)

#define D3DCOLOR_NONE D3DCOLORMODEL::NONE
#define D3DCOLOR_RGB D3DCOLORMODEL::RGB
#define D3DCOLOR_MONO D3DCOLORMODEL::MONO
enum class D3DCOLORMODEL {
	NONE,
	RGB,
	MONO,
};

// --- Structs ---
struct D3DVECTOR {
	float x, y, z;
};

struct D3DDEVICEDESC {
	D3DDeviceDescFlags dwFlags;
	DDBitDepths dwDeviceZBufferBitDepth;
	D3DCOLORMODEL dcmColorModel; // D3DCOLOR_* Bit flag, but Isle think it's an enum
	DDBitDepths dwDeviceRenderBitDepth;
	struct {
		D3DPShadeCaps dwShadeCaps;
		D3DPTextureCaps dwTextureCaps;
		D3DPTextureFilterCaps dwTextureFilterCaps;
	} dpcTriCaps;
};
typedef D3DDEVICEDESC* LPD3DDEVICEDESC;

struct IDirect3DDevice2 : virtual public IUnknown {};

typedef HRESULT (*LPD3DENUMDEVICESCALLBACK)(LPGUID, LPSTR, LPSTR, LPD3DDEVICEDESC, LPD3DDEVICEDESC, LPVOID);
struct IDirect3D2 : virtual public IUnknown {
	virtual HRESULT CreateDevice(
		const GUID& guid,
		IDirectDrawSurface* pBackBuffer,
		IDirect3DDevice2** ppDirect3DDevice
	) = 0;
	virtual HRESULT EnumDevices(LPD3DENUMDEVICESCALLBACK cb, void* ctx) = 0;
};
typedef IDirect3D2* LPDIRECT3D2;
