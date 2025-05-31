#include "detectdx5.h"

#ifdef MINIWIN
#include "miniwin/ddraw.h"
#include "miniwin/dinput.h"
#include "qoperatingsystemversion.h"
#include "qlibrary.h"
#else
#include <ddraw.h>
#include <dinput.h>
#endif

typedef struct IUnknown* LPUNKNOWN;

typedef HRESULT WINAPI DirectDrawCreate_fn(GUID FAR* lpGUID, LPDIRECTDRAW FAR* lplpDD, IUnknown FAR* pUnkOuter);
typedef HRESULT WINAPI
DirectInputCreateA_fn(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUTA* ppDI, LPUNKNOWN punkOuter);

// FUNCTION: CONFIG 0x004048f0
BOOL DetectDirectX5()
{
	unsigned int version;
	BOOL found;
	DetectDirectX(&version, &found);
	return version >= 0x500;
}

// FUNCTION: CONFIG 0x00404920
void DetectDirectX(unsigned int* p_version, BOOL* p_found)
{
    QOperatingSystemVersion os_version = QOperatingSystemVersion::current();

	if (os_version.type() == QOperatingSystemVersion::Unknown) {
		*p_version = 0;
		*p_found = 0;
		return;
	}
	if (os_version.type() == QOperatingSystemVersion::Windows) {
		*p_found = 2;
		if (os_version.majorVersion() < 4) {
			*p_found = 0;
			return;
		}
		if (os_version.majorVersion() != 4) {
			*p_version = 0x501;
			return;
		}
		*p_version = 0x200;
		QLibrary dinput_module("DINPUT.DLL");
		dinput_module.load();
		if (!dinput_module.isLoaded()) {
			QT_DEBUG("Couldn't LoadLibrary DInput\r\n");
			return;
		}
		DirectInputCreateA_fn* func_DirectInputCreateA =
			reinterpret_cast<DirectInputCreateA_fn*>(dinput_module.resolve("DirectInputCreateA"));
		dinput_module.unload();
		if (!func_DirectInputCreateA) {
			QT_DEBUG("Couldn't GetProcAddress DInputCreate\r\n");
			return;
		}
		*p_version = 0x300;
		return;
	}
	*p_found = 1;
	if (os_version.majorVersion() >= 0x550) {
		QT_DEBUG("what is this for??");
		*p_version = 0x501;
		return;
	}
	QLibrary ddraw_module("DDRAW.DLL");
	ddraw_module.load();
	if (!ddraw_module.isLoaded()) {
		*p_version = 0;
		*p_found = 0;
		ddraw_module.unload();
		return;
	}
	DirectDrawCreate_fn* func_DirectDrawCreate =
		reinterpret_cast<DirectDrawCreate_fn*>(ddraw_module.resolve("DirectDrawCreate"));
	if (!func_DirectDrawCreate) {
		*p_version = 0;
		*p_found = 0;
		ddraw_module.unload();
		QT_DEBUG("Couldn't LoadLibrary DDraw\r\n");
		return;
	}
	LPDIRECTDRAW ddraw;
	if (func_DirectDrawCreate(NULL, &ddraw, NULL) < 0) {
		*p_version = 0;
		*p_found = 0;
		ddraw_module.unload();
		QT_DEBUG("Couldn't create DDraw\r\n");
		return;
	}
	*p_version = 0x100;
	LPDIRECTDRAW2 ddraw2;
	if (ddraw->QueryInterface(IID_IDirectDraw2, (LPVOID*) &ddraw2) < 0) {
		ddraw->Release();
		ddraw_module.unload();
		QT_DEBUG("Couldn't QI DDraw2\r\n");
		return;
	}
	ddraw->Release();
	*p_version = 0x200;
	QLibrary dinput_module("DINPUT.DLL");
	dinput_module.load();
	if (!dinput_module.isLoaded()) {
		QT_DEBUG("Couldn't LoadLibrary DInput\r\n");
		ddraw2->Release();
		ddraw_module.unload();
		return;
	}
	DirectInputCreateA_fn* func_DirectInputCreateA =
		reinterpret_cast<DirectInputCreateA_fn*>(dinput_module.resolve("DirectInputCreateA"));
	dinput_module.unload();
	if (!func_DirectInputCreateA) {
		ddraw_module.unload();
		ddraw2->Release();
		QT_DEBUG("Couldn't GetProcAddress DInputCreate\r\n");
		return;
	}
	*p_version = 0x300;
	DDSURFACEDESC surface_desc;
	memset(&surface_desc, 0, sizeof(surface_desc));
	surface_desc.dwSize = sizeof(surface_desc);
	surface_desc.dwFlags = DDSD_CAPS;
	surface_desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	if (ddraw2->SetCooperativeLevel(NULL, DDSCL_NORMAL) < 0) {
		ddraw2->Release();
		ddraw_module.unload();
		*p_version = 0;
		QT_DEBUG("Couldn't Set coop level\r\n");
		return;
	}
	LPDIRECTDRAWSURFACE surface;
	if (ddraw2->CreateSurface(&surface_desc, &surface, NULL) < 0) {
		ddraw2->Release();
		ddraw_module.unload();
		*p_version = 0;
		QT_DEBUG("Couldn't CreateSurface\r\n");
		return;
	}
	LPDIRECTDRAWSURFACE3 surface3;
	if (surface->QueryInterface(IID_IDirectDrawSurface3, reinterpret_cast<LPVOID*>(&surface3)) < 0) {
		ddraw2->Release();
		ddraw_module.unload();
		return;
	}
	*p_version = 0x500;
	surface3->Release();
	ddraw2->Release();
	ddraw_module.unload();
}
