#include "dyn_d3dxof.h"

static enum {
  DYN_D3DXOF_INIT = 0,
  DYN_D3DXOF_SUCCESS = 1,
} g_dyn_d3dxof_state = DYN_D3DXOF_INIT;
static HMODULE g_d3dxof;
static HRESULT (STDAPICALLTYPE * g_DynamicDirectXFileCreate)(LPDIRECTXFILE *lplpDirectXFile);

static void init_dyn_d3d(void) {
    if (g_dyn_d3dxof_state == DYN_D3DXOF_SUCCESS) {
        return;
    }
    g_d3dxof = LoadLibraryA("d3dxof.dll");
    if (g_d3dxof == NULL) {
        MessageBoxA(NULL, "Cannot find d3dxof.dll", "Cannot find d3dxof.dll", MB_ICONERROR);
        abort();
    }
    g_DynamicDirectXFileCreate = (void*)GetProcAddress(g_d3dxof, "DirectXFileCreate");
    if (g_d3dxof == NULL) {
        MessageBoxA(NULL, "Missing symbols", "d3dxof.dll misses DirectXFileCreate", MB_ICONERROR);
        abort();
    }
    g_dyn_d3dxof_state = DYN_D3DXOF_SUCCESS;
}


STDAPI DynamicDirectXFileCreate(LPDIRECTXFILE *lplpDirectXFile)
{
    init_dyn_d3d();
    return g_DynamicDirectXFileCreate(lplpDirectXFile);
}
