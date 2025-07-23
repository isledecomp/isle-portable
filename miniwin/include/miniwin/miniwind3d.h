#pragma once

DEFINE_GUID(IID_IDirect3DMiniwin, 0xf8a97f2d, 0x9b3a, 0x4f1c, 0x9e, 0x8d, 0x6a, 0x5b, 0x4c, 0x3d, 0x2e, 0x1f);

struct IDirect3DMiniwin : virtual public IUnknown {
	virtual HRESULT RequestMSAA(DWORD msaaSamples) = 0;
	virtual DWORD GetMSAASamples() const = 0;
	virtual HRESULT RequestAnisotropic(float anisotropic) = 0;
	virtual float GetAnisotropic() const = 0;
};
