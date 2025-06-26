#pragma once

#include <SDL3/SDL_log.h>
#include <psp2/gxm.h>
#include <psp2/kernel/clib.h>

#define SCE_ERR(func, ...) ({ \
    sceClibPrintf(#func "\n"); \
    int __sce_err_ret_val = func(__VA_ARGS__); \
    if (__sce_err_ret_val < 0) { \
        sceClibPrintf(#func " error: 0x%x\n", __sce_err_ret_val); \
    } \
    __sce_err_ret_val < 0; \
})

#define ALIGN(x, a) (((x) + ((a)-1)) & ~((a)-1))

#define ALIGNMENT(n, a) (((a) - ((n) % (a))) % (a))


#define SET_UNIFORM(buffer, param, value, program) \
    do { \
        size_t __offset = sceGxmProgramParameterGetResourceIndex(param); \
        void* __dst = (uint8_t*)(buffer) + (__offset * sizeof(uint32_t)); \
		memcpy(__dst, reinterpret_cast<const void*>(&(value)), sizeof(value)); \
        /*SDL_Log("set uniform param=%s offset=%d size=%d buffer_size=%d", \
                sceGxmProgramParameterGetName(param), __offset*4, sizeof(value), sceGxmProgramGetDefaultUniformBufferSize(program));*/ \
    } while (0)

#define GET_SHADER_PARAM(var, gxp, name, ret) \
	const SceGxmProgramParameter* var = sceGxmProgramFindParameterByName(gxp, name); \
	if(!var) { \
		SDL_Log("Failed to find param %s", name); \
		return ret; \
	}
