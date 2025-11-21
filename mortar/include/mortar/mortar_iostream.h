#pragma once

#include "mortar_begin.h"

#include <stddef.h>
#include <stdint.h>

typedef struct MORTAR_IOStream MORTAR_IOStream;

typedef enum MORTAR_IOWhence {
	MORTAR_IO_SEEK_SET,
	MORTAR_IO_SEEK_CUR,
	MORTAR_IO_SEEK_END
} MORTAR_IOWhence;

typedef enum MORTAR_IOStatus {
	MORTAR_IO_STATUS_READY,
	MORTAR_IO_STATUS_ERROR,
	MORTAR_IO_STATUS_EOF,
	MORTAR_IO_STATUS_NOT_READY,
	MORTAR_IO_STATUS_READONLY,
	MORTAR_IO_STATUS_WRITEONLY
} MORTAR_IOStatus;

extern MORTAR_DECLSPEC MORTAR_IOStream* MORTAR_IOFromFile(const char* file, const char* mode);

extern MORTAR_DECLSPEC MORTAR_IOStream* MORTAR_IOFromMem(void* mem, size_t size);

extern MORTAR_DECLSPEC size_t MORTAR_ReadIO(MORTAR_IOStream* context, void* ptr, size_t size);

extern MORTAR_DECLSPEC size_t MORTAR_WriteIO(MORTAR_IOStream* context, const void* ptr, size_t size);

extern MORTAR_DECLSPEC int64_t MORTAR_SeekIO(MORTAR_IOStream* context, int64_t offset, MORTAR_IOWhence whence);

extern MORTAR_DECLSPEC int64_t MORTAR_TellIO(MORTAR_IOStream* context);

extern MORTAR_DECLSPEC int64_t MORTAR_GetIOSize(MORTAR_IOStream* context);

extern MORTAR_DECLSPEC bool MORTAR_CloseIO(MORTAR_IOStream* context);

extern MORTAR_DECLSPEC MORTAR_IOStatus MORTAR_GetIOStatus(MORTAR_IOStream* context);

extern MORTAR_DECLSPEC void* MORTAR_LoadFile(const char* file, size_t* datasize);

#include "mortar_end.h"
