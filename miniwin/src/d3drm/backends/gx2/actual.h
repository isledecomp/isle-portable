#pragma once
#include <gx2/mem.h>
#include <gx2r/buffer.h>

inline uint32_t GX2RGetGpuAddr(const GX2RBuffer* buffer)
{
	return (uint32_t) buffer->buffer;
}
