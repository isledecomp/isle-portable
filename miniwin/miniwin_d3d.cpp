#include "miniwin_d3d.h"

HRESULT IDirect3D2::EnumDevices(LPD3DENUMDEVICESCALLBACK cb, void* ctx)
{
	if (!cb) {
		return DDERR_INVALIDPARAMS;
	}

	GUID deviceGuid = {0xa4665c, 0x2673, 0x11ce, 0x8034a0};

	char* deviceName = (char*) "MiniWin 3D Device";
	char* deviceDesc = (char*) "Stubbed 3D device";

	D3DDEVICEDESC halDesc = {};
	halDesc.dcmColorModel = D3DCOLOR_RGB;
	halDesc.dwDeviceZBufferBitDepth = DDBD_16;
	halDesc.dpcTriCaps.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE;
	D3DDEVICEDESC helDesc = {};
	halDesc.dcmColorModel = D3DCOLOR_RGB;
	halDesc.dwDeviceZBufferBitDepth = DDBD_16;
	halDesc.dpcTriCaps.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE;

	cb(&deviceGuid, deviceName, deviceDesc, &halDesc, &helDesc, ctx);

	return S_OK;
}
