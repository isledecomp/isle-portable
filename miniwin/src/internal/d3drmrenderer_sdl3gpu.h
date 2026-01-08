#pragma once

#include "d3drmrenderer.h"
#include "d3drmtexture_impl.h"
#include "ddraw_impl.h"
#include "ddsurface_impl.h"

#include <vector>

DEFINE_GUID(SDL3_GPU_GUID, 0x682656F3, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01);

extern Direct3DRMRenderer* Create_Direct3DRMSDL3GPURenderer(DWORD width, DWORD height);

extern void Direct3DRMSDL3GPU_EnumDevice(LPD3DENUMDEVICESCALLBACK cb, void* ctx);
