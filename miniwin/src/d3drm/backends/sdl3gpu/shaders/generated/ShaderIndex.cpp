// clang-format off

// +==============================================+
// | This file is auto-generated, do not edit it. |
// +==============================================+

#include "ShaderIndex.h"

#include "PositionColor.vert.h"
#include "SolidColor.frag.h"

#if defined(SDL_PLATFORM_WINDOWS)
static const SDL_GPUShaderCreateInfo VertexShaderDXILCodes[] = {
  // VertexShaderId::SolidColor
  {
    /* code_size */             sizeof(PositionColor_vert_dxil),
    /* code */                  PositionColor_vert_dxil,
    /* entrypoint */            "main",
    /* format */                SDL_GPU_SHADERFORMAT_DXIL,
    /* stage */                 SDL_GPU_SHADERSTAGE_VERTEX,
    /* num_samplers */          0,
    /* num_storage_textures */  0,
    /* num_storage_buffers */   0,
    /* num_uniform_buffers */   1,
  },
};
static const SDL_GPUShaderCreateInfo VertexShaderDXBCCodes[] = {
  // VertexShaderId::PositionColor
  {
    /* code_size */             sizeof(PositionColor_vert_dxbc),
    /* code */                  PositionColor_vert_dxbc,
    /* entrypoint */            "main",
    /* format */                SDL_GPU_SHADERFORMAT_DXBC,
    /* stage */                 SDL_GPU_SHADERSTAGE_VERTEX,
    /* num_samplers */          0,
    /* num_storage_textures */  0,
    /* num_storage_buffers */   0,
    /* num_uniform_buffers */   1,
  },
};
#endif

#if defined(SDL_PLATFORM_APPLE)
static const SDL_GPUShaderCreateInfo VertexShaderMSLCodes[] = {
  {
    /* code_size */             sizeof(PositionColor_vert_msl),
    /* code */                  PositionColor_vert_msl,
    /* entrypoint */            "main0",
    /* format */                SDL_GPU_SHADERFORMAT_MSL,
    /* stage */                 SDL_GPU_SHADERSTAGE_VERTEX,
    /* num_samplers */          0,
    /* num_storage_textures */  0,
    /* num_storage_buffers */   0,
    /* num_uniform_buffers */   1,
  },
};
#endif

static const SDL_GPUShaderCreateInfo VertexShaderSPIRVCodes[] = {
  {
    /* code_size */             sizeof(PositionColor_vert_spirv),
    /* code */                  PositionColor_vert_spirv,
    /* entrypoint */            "main",
    /* format */                SDL_GPU_SHADERFORMAT_SPIRV,
    /* stage */                 SDL_GPU_SHADERSTAGE_VERTEX,
    /* num_samplers */          0,
    /* num_storage_textures */  0,
    /* num_storage_buffers */   0,
    /* num_uniform_buffers */   1,
  },
};

#if defined(SDL_PLATFORM_WINDOWS)
static const SDL_GPUShaderCreateInfo FragmentShaderDXILCodes[] = {
  // FragmentShaderId::PositionColor
  {
    /* code_size */             sizeof(SolidColor_frag_dxil),
    /* code */                  SolidColor_frag_dxil,
    /* entrypoint */            "main",
    /* format */                SDL_GPU_SHADERFORMAT_DXIL,
    /* stage */                 SDL_GPU_SHADERSTAGE_FRAGMENT,
    /* num_samplers */          1,
    /* num_storage_textures */  0,
    /* num_storage_buffers */   0,
    /* num_uniform_buffers */   1,
  },
};
static const SDL_GPUShaderCreateInfo FragmentShaderDXBCCodes[] = {
  // FragmentShaderId::SolidColor
  {
    /* code_size */             sizeof(SolidColor_frag_dxbc),
    /* code */                  SolidColor_frag_dxbc,
    /* entrypoint */            "main",
    /* format */                SDL_GPU_SHADERFORMAT_DXBC,
    /* stage */                 SDL_GPU_SHADERSTAGE_FRAGMENT,
    /* num_samplers */          1,
    /* num_storage_textures */  0,
    /* num_storage_buffers */   0,
    /* num_uniform_buffers */   1,
  },
};
#endif

#if defined(SDL_PLATFORM_APPLE)
static const SDL_GPUShaderCreateInfo FragmentShaderMSLCodes[] = {
  {
    /* code_size */             sizeof(SolidColor_frag_msl),
    /* code */                  SolidColor_frag_msl,
    /* entrypoint */            "main0",
    /* format */                SDL_GPU_SHADERFORMAT_MSL,
    /* stage */                 SDL_GPU_SHADERSTAGE_FRAGMENT,
    /* num_samplers */          1,
    /* num_storage_textures */  0,
    /* num_storage_buffers */   0,
    /* num_uniform_buffers */   1,
  },
};
#endif

static const SDL_GPUShaderCreateInfo FragmentShaderSPIRVCodes[] = {
  {
    /* code_size */             sizeof(SolidColor_frag_spirv),
    /* code */                  SolidColor_frag_spirv,
    /* entrypoint */            "main",
    /* format */                SDL_GPU_SHADERFORMAT_SPIRV,
    /* stage */                 SDL_GPU_SHADERSTAGE_FRAGMENT,
    /* num_samplers */          1,
    /* num_storage_textures */  0,
    /* num_storage_buffers */   0,
    /* num_uniform_buffers */   1,
  },
};

const SDL_GPUShaderCreateInfo* GetVertexShaderCode(VertexShaderId id, SDL_GPUShaderFormat formats) {
#if defined(SDL_PLATFORM_WINDOWS)
  if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
    SDL_assert(id < SDL_arraysize(VertexShaderDXILCodes));
    return &VertexShaderDXILCodes[id];
  }
  if (formats & SDL_GPU_SHADERFORMAT_DXBC) {
    SDL_assert(id < SDL_arraysize(VertexShaderDXBCCodes));
    return &VertexShaderDXBCCodes[id];
  }
#endif
#if defined(SDL_PLATFORM_APPLE)
  if (formats & SDL_GPU_SHADERFORMAT_MSL) {
    SDL_assert(id < SDL_arraysize(VertexShaderMSLCodes));
    return &VertexShaderMSLCodes[id];
  }
#endif
  if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
    SDL_assert(id < SDL_arraysize(VertexShaderSPIRVCodes));
    return &VertexShaderSPIRVCodes[id];
  }
  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not find vertex shader code for id=%d and formats=0x%x", id, formats);
  return nullptr;
}

const SDL_GPUShaderCreateInfo* GetFragmentShaderCode(FragmentShaderId id, SDL_GPUShaderFormat formats) {
#if defined(SDL_PLATFORM_WINDOWS)
  if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
    SDL_assert(id < SDL_arraysize(FragmentShaderDXILCodes));
    return &FragmentShaderDXILCodes[id];
  }
  if (formats & SDL_GPU_SHADERFORMAT_DXBC) {
    SDL_assert(id < SDL_arraysize(FragmentShaderDXBCCodes));
    return &FragmentShaderDXBCCodes[id];
  }
#endif
#if defined(SDL_PLATFORM_APPLE)
  if (formats & SDL_GPU_SHADERFORMAT_MSL) {
    SDL_assert(id < SDL_arraysize(FragmentShaderMSLCodes));
    return &FragmentShaderMSLCodes[id];
  }
#endif
  if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
    SDL_assert(id < SDL_arraysize(FragmentShaderSPIRVCodes));
    return &FragmentShaderSPIRVCodes[id];
  }
  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not find fragment shader code for id=%d and formats=0x%x", id, formats);
  return nullptr;
}
