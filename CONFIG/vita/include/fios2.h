#include <stdint.h>
#include <stddef.h>

#include <psp2/kernel/threadmgr.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct SceFiosBuffer {
	void *pPtr;
	size_t length;
} SceFiosBuffer;

#define SCE_FIOS_THREAD_TYPES 3

typedef struct SceFiosParams
{
	uint32_t initialized : 1;
	uint32_t paramsSize : 15;
	uint32_t pathMax : 16;
	uint32_t profiling;
	uint32_t ioThreadCount;
	uint32_t threadsPerScheduler;
	uint32_t extraFlag1 : 1;
	uint32_t extraFlags : 31;
	uint32_t maxChunk;
	uint8_t  maxDecompressorThreadCount;
	uint8_t  reserved1;
	uint8_t  reserved2;
	uint8_t  reserved3;
	intptr_t reserved4;
	intptr_t reserved5;
	SceFiosBuffer opStorage;
	SceFiosBuffer fhStorage;
	SceFiosBuffer dhStorage;
	SceFiosBuffer chunkStorage;
	void* pVprintf;
	void*  pMemcpy;
	void* pProfileCallback;
	int    threadPriority[SCE_FIOS_THREAD_TYPES];
	int    threadAffinity[SCE_FIOS_THREAD_TYPES];
	int    threadStackSize[SCE_FIOS_THREAD_TYPES];
} SceFiosParams;

#define SCE_KERNEL_HIGHEST_PRIORITY_USER				(64)
#define SCE_KERNEL_LOWEST_PRIORITY_USER					(191)

#define SCE_FIOS_IO_THREAD_DEFAULT_PRIORITY            (SCE_KERNEL_HIGHEST_PRIORITY_USER+2)
#define SCE_FIOS_DECOMPRESSOR_THREAD_DEFAULT_PRIORITY  (SCE_KERNEL_LOWEST_PRIORITY_USER-2)
#define SCE_FIOS_CALLBACK_THREAD_DEFAULT_PRIORITY      (SCE_KERNEL_HIGHEST_PRIORITY_USER+2)

#define SCE_FIOS_THREAD_DEFAULT_AFFINITY               SCE_KERNEL_CPU_MASK_USER_2
#define SCE_FIOS_IO_THREAD_DEFAULT_AFFINITY            SCE_FIOS_THREAD_DEFAULT_AFFINITY
#define SCE_FIOS_DECOMPRESSOR_THREAD_DEFAULT_AFFINITY  SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT
#define SCE_FIOS_CALLBACK_THREAD_DEFAULT_AFFINITY      SCE_FIOS_THREAD_DEFAULT_AFFINITY

#define SCE_FIOS_IO_THREAD_DEFAULT_STACKSIZE                   (8*1024)
#define SCE_FIOS_DECOMPRESSOR_THREAD_DEFAULT_STACKSIZE         (16*1024)
#define SCE_FIOS_CALLBACK_THREAD_DEFAULT_STACKSIZE             (8*1024)

#define SCE_FIOS_PARAMS_INITIALIZER   { 0, sizeof(SceFiosParams), 0, 0, \
		2, 2, \
		0, 0, \
		(256*1024), \
		2, 0, 0, 0, 0, 0, \
		{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, \
		NULL, NULL, NULL, \
		{ SCE_FIOS_IO_THREAD_DEFAULT_PRIORITY, SCE_FIOS_DECOMPRESSOR_THREAD_DEFAULT_PRIORITY, SCE_FIOS_CALLBACK_THREAD_DEFAULT_PRIORITY }, \
		{ SCE_FIOS_IO_THREAD_DEFAULT_AFFINITY, SCE_FIOS_DECOMPRESSOR_THREAD_DEFAULT_AFFINITY, SCE_FIOS_CALLBACK_THREAD_DEFAULT_AFFINITY}, \
		{ SCE_FIOS_IO_THREAD_DEFAULT_STACKSIZE, SCE_FIOS_DECOMPRESSOR_THREAD_DEFAULT_STACKSIZE, SCE_FIOS_CALLBACK_THREAD_DEFAULT_STACKSIZE}}

#define SCE_FIOS_IO_THREAD 0
#define SCE_FIOS_DECOMPRESSOR_THREAD 1
#define SCE_FIOS_CALLBACK_THREAD 2

#define SCE_FIOS_FH_SIZE	80
#define SCE_FIOS_DH_SIZE	80
#define SCE_FIOS_OP_SIZE	168
#define SCE_FIOS_CHUNK_SIZE	64

#define SCE_FIOS_ALIGN_UP(val,align)      (((val) + ((align)-1)) & ~((align)-1))

#define SCE_FIOS_STORAGE_SIZE(num, size) \
	(((num) * (size)) + SCE_FIOS_ALIGN_UP(SCE_FIOS_ALIGN_UP((num), 8) / 8, 8))

#define SCE_FIOS_DH_STORAGE_SIZE(numDHs, pathMax) \
	SCE_FIOS_STORAGE_SIZE(numDHs, SCE_FIOS_DH_SIZE + pathMax)

#define SCE_FIOS_FH_STORAGE_SIZE(numFHs,pathMax) \
	SCE_FIOS_STORAGE_SIZE(numFHs, SCE_FIOS_FH_SIZE + pathMax)

#define SCE_FIOS_OP_STORAGE_SIZE(numOps,pathMax) \
	SCE_FIOS_STORAGE_SIZE(numOps, SCE_FIOS_OP_SIZE + pathMax)

#define SCE_FIOS_CHUNK_STORAGE_SIZE(numChunks) \
	SCE_FIOS_STORAGE_SIZE(numChunks, SCE_FIOS_CHUNK_SIZE)

int sceFiosInitialize(SceFiosParams* params);


typedef int64_t SceFiosTime;

typedef int32_t SceFiosHandle;

typedef SceFiosHandle SceFiosFH;

typedef struct SceFiosOpenParams
{
	uint32_t        openFlags:16;
	uint32_t        opFlags:16;
	uint32_t        reserved;
	SceFiosBuffer   buffer;
} SceFiosOpenParams;

typedef struct SceFiosOpAttr
{
	SceFiosTime       deadline;
	void* pCallback;
	void *            pCallbackContext;
	int32_t           priority : 8;
	uint32_t          opflags : 24;
	uint32_t          userTag;
	void *            userPtr;
	void *            pReserved;
} SceFiosOpAttr;


int       sceFiosFHOpenWithModeSync(const SceFiosOpAttr *pAttr, SceFiosFH *pOutFH, const char *pPath, const SceFiosOpenParams *pOpenParams, int32_t nativeMode);



#ifdef __cplusplus
}
#endif
