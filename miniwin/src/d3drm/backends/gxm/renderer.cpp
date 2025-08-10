#include "d3drmrenderer_gxm.h"
#include "gxm_context.h"
#include "gxm_memory.h"
#include "meshutils.h"
#include "razor.h"
#include "tlsf.h"
#include "utils.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <psp2/common_dialog.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/types.h>
#include <string>
#define INCBIN_PREFIX _inc_
#include "incbin.h"

static bool with_razor_capture = false;
static bool with_razor_hud = false;
static bool gxm_initialized = false;

// from isleapp
extern bool g_dpadUp;
extern bool g_dpadDown;
extern bool g_dpadLeft;
extern bool g_dpadRight;

#define VITA_GXM_SCREEN_WIDTH 960
#define VITA_GXM_SCREEN_HEIGHT 544
#define VITA_GXM_SCREEN_STRIDE 1024
#define VITA_GXM_PENDING_SWAPS 2
#define CDRAM_POOL_SIZE 64 * 1024 * 1024

#define VITA_GXM_COLOR_FORMAT SCE_GXM_COLOR_FORMAT_A8B8G8R8
#define VITA_GXM_PIXEL_FORMAT SCE_DISPLAY_PIXELFORMAT_A8B8G8R8
const SceGxmMultisampleMode msaaMode = SCE_GXM_MULTISAMPLE_NONE;

#define SCE_GXM_PRECOMPUTED_ALIGNMENT 16

#define INCSHADER(filename, name)                                                                                      \
	INCBIN(name, filename);                                                                                            \
	const SceGxmProgram* name = (const SceGxmProgram*) _inc_##name##Data;

INCSHADER("shaders/main.vert.gxp", mainVertexProgramGxp);
INCSHADER("shaders/main.color.frag.gxp", mainColorFragmentProgramGxp);
INCSHADER("shaders/main.texture.frag.gxp", mainTextureFragmentProgramGxp);

INCSHADER("shaders/plane.vert.gxp", planeVertexProgramGxp);
INCSHADER("shaders/image.frag.gxp", imageFragmentProgramGxp);
INCSHADER("shaders/color.frag.gxp", colorFragmentProgramGxp);

static const SceGxmBlendInfo blendInfoOpaque = {
	.colorMask = SCE_GXM_COLOR_MASK_ALL,
	.colorFunc = SCE_GXM_BLEND_FUNC_NONE,
	.alphaFunc = SCE_GXM_BLEND_FUNC_NONE,
	.colorSrc = SCE_GXM_BLEND_FACTOR_ZERO,
	.colorDst = SCE_GXM_BLEND_FACTOR_ZERO,
	.alphaSrc = SCE_GXM_BLEND_FACTOR_ZERO,
	.alphaDst = SCE_GXM_BLEND_FACTOR_ZERO,
};

static const SceGxmBlendInfo blendInfoTransparent = {
	.colorMask = SCE_GXM_COLOR_MASK_ALL,
	.colorFunc = SCE_GXM_BLEND_FUNC_ADD,
	.alphaFunc = SCE_GXM_BLEND_FUNC_ADD,
	.colorSrc = SCE_GXM_BLEND_FACTOR_SRC_ALPHA,
	.colorDst = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	.alphaSrc = SCE_GXM_BLEND_FACTOR_ONE,
	.alphaDst = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
};

typedef struct GXMVertex {
	float position[3];
	float normal[3];
	float texCoord[2];
} GXMVertex;

typedef struct GXMDisplayData {
	void* address;
	int index;
} GXMDisplayData;

static void display_callback(const void* callback_data)
{
	const GXMDisplayData* display_data = (const GXMDisplayData*) callback_data;

	GXMRenderer* renderer = static_cast<GXMRenderer*>(DDRenderer);
	renderer->DeleteTextures(display_data->index);

	SceDisplayFrameBuf framebuf;
	SDL_memset(&framebuf, 0x00, sizeof(SceDisplayFrameBuf));
	framebuf.size = sizeof(SceDisplayFrameBuf);
	framebuf.base = display_data->address;
	framebuf.pitch = VITA_GXM_SCREEN_STRIDE;
	framebuf.pixelformat = VITA_GXM_PIXEL_FORMAT;
	framebuf.width = VITA_GXM_SCREEN_WIDTH;
	framebuf.height = VITA_GXM_SCREEN_HEIGHT;
	sceDisplaySetFrameBuf(&framebuf, SCE_DISPLAY_SETBUF_NEXTFRAME);
	sceDisplayWaitSetFrameBuf();
}

#ifdef GXM_WITH_RAZOR
#include <taihen.h>

static int load_skprx(const char* name)
{
	int modid = taiLoadKernelModule(name, 0, nullptr);
	if (modid < 0) {
		sceClibPrintf("%s load: 0x%08x\n", name, modid);
		return modid;
	}
	int status;
	int ret = taiStartKernelModule(modid, 0, nullptr, 0, nullptr, &status);
	if (ret < 0) {
		sceClibPrintf("%s start: 0x%08x\n", name, ret);
	}
	return ret;
}

static int load_suprx(const char* name)
{
	sceClibPrintf("loading %s\n", name);
	int modid = _sceKernelLoadModule(name, 0, nullptr);
	if (modid < 0) {
		sceClibPrintf("%s load: 0x%08x\n", name, modid);
		return modid;
	}
	int status;
	int ret = sceKernelStartModule(modid, 0, nullptr, 0, nullptr, &status);
	if (ret < 0) {
		sceClibPrintf("%s start: 0x%08x\n", name, ret);
	}
	return ret;
}

static void load_razor()
{
	if (load_suprx("app0:librazorcapture_es4.suprx") >= 0) {
		with_razor_capture = true;
	}

	if (with_razor_capture) {
		// sceRazorGpuCaptureEnableSalvage("ux0:data/gpu_crash.sgx");
	}

	if (with_razor_hud) {
		sceRazorGpuTraceSetFilename("ux0:data/gpu_trace", 3);
	}
}
#endif

int gxm_library_init()
{
	if (gxm_initialized) {
		return 0;
	}

#ifdef GXM_WITH_RAZOR
	load_razor();
#endif

	SceGxmInitializeParams initializeParams;
	SDL_memset(&initializeParams, 0, sizeof(SceGxmInitializeParams));
	initializeParams.flags = 0;
	initializeParams.displayQueueMaxPendingCount = VITA_GXM_PENDING_SWAPS;
	initializeParams.displayQueueCallback = display_callback;
	initializeParams.displayQueueCallbackDataSize = sizeof(GXMDisplayData);
	initializeParams.parameterBufferSize = SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE;

	int err = sceGxmInitialize(&initializeParams);
	if (err != 0) {
		SDL_LogError(SDL_LOG_CATEGORY_RENDER, "gxm init failed: %d", err);
		return err;
	}
	gxm_initialized = true;
	return 0;
}

GXMContext* gxm;

int GXMContext::init()
{
	if (this->context) {
		return 0;
	}

	int ret = gxm_library_init();
	if (ret < 0) {
		return ret;
	}

	const unsigned int patcherBufferSize = 64 * 1024;
	const unsigned int patcherVertexUsseSize = 64 * 1024;
	const unsigned int patcherFragmentUsseSize = 64 * 1024;

	const uint32_t alignedWidth = ALIGN(VITA_GXM_SCREEN_WIDTH, SCE_GXM_TILE_SIZEX);
	const uint32_t alignedHeight = ALIGN(VITA_GXM_SCREEN_HEIGHT, SCE_GXM_TILE_SIZEY);
	uint32_t sampleCount = alignedWidth * alignedHeight;
	uint32_t depthStrideInSamples = alignedWidth;

	if (msaaMode == SCE_GXM_MULTISAMPLE_4X) {
		sampleCount *= 4;
		depthStrideInSamples *= 2;
	}
	else if (msaaMode == SCE_GXM_MULTISAMPLE_2X) {
		sampleCount *= 2;
	}

	// allocate buffers
	this->vdmRingBuffer = vita_mem_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&this->vdmRingBufferUid,
		"vdmRingBuffer"
	);

	this->vertexRingBuffer = vita_mem_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&this->vertexRingBufferUid,
		"vertexRingBuffer"
	);

	this->fragmentRingBuffer = vita_mem_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&this->fragmentRingBufferUid,
		"fragmentRingBuffer"
	);

	this->fragmentUsseRingBuffer = vita_mem_fragment_usse_alloc(
		SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE,
		&this->fragmentUsseRingBufferUid,
		&this->fragmentUsseRingBufferOffset
	);

	// create context
	SceGxmContextParams contextParams;
	memset(&contextParams, 0, sizeof(SceGxmContextParams));
	contextParams.hostMem = SDL_malloc(SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE);
	contextParams.hostMemSize = SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE;
	contextParams.vdmRingBufferMem = this->vdmRingBuffer;
	contextParams.vdmRingBufferMemSize = SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE;
	contextParams.vertexRingBufferMem = this->vertexRingBuffer;
	contextParams.vertexRingBufferMemSize = SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE;
	contextParams.fragmentRingBufferMem = this->fragmentRingBuffer;
	contextParams.fragmentRingBufferMemSize = SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE;
	contextParams.fragmentUsseRingBufferMem = this->fragmentUsseRingBuffer;
	contextParams.fragmentUsseRingBufferMemSize = SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE;
	contextParams.fragmentUsseRingBufferOffset = this->fragmentUsseRingBufferOffset;

	ret = SCE_ERR(sceGxmCreateContext, &contextParams, &this->context);
	if (ret < 0) {
		return ret;
	}
	this->contextHostMem = contextParams.hostMem;

	// shader patcher
	this->patcherBuffer = vita_mem_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		patcherBufferSize,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		&this->patcherBufferUid,
		"patcherBuffer"
	);

	this->patcherVertexUsse =
		vita_mem_vertex_usse_alloc(patcherVertexUsseSize, &this->patcherVertexUsseUid, &this->patcherVertexUsseOffset);

	this->patcherFragmentUsse = vita_mem_fragment_usse_alloc(
		patcherFragmentUsseSize,
		&this->patcherFragmentUsseUid,
		&this->patcherFragmentUsseOffset
	);

	SceGxmShaderPatcherParams patcherParams;
	memset(&patcherParams, 0, sizeof(SceGxmShaderPatcherParams));
	patcherParams.userData = NULL;
	patcherParams.hostAllocCallback = &patcher_host_alloc;
	patcherParams.hostFreeCallback = &patcher_host_free;
	patcherParams.bufferAllocCallback = NULL;
	patcherParams.bufferFreeCallback = NULL;
	patcherParams.bufferMem = this->patcherBuffer;
	patcherParams.bufferMemSize = patcherBufferSize;
	patcherParams.vertexUsseAllocCallback = NULL;
	patcherParams.vertexUsseFreeCallback = NULL;
	patcherParams.vertexUsseMem = this->patcherVertexUsse;
	patcherParams.vertexUsseMemSize = patcherVertexUsseSize;
	patcherParams.vertexUsseOffset = this->patcherVertexUsseOffset;
	patcherParams.fragmentUsseAllocCallback = NULL;
	patcherParams.fragmentUsseFreeCallback = NULL;
	patcherParams.fragmentUsseMem = this->patcherFragmentUsse;
	patcherParams.fragmentUsseMemSize = patcherFragmentUsseSize;
	patcherParams.fragmentUsseOffset = this->patcherFragmentUsseOffset;

	ret = SCE_ERR(sceGxmShaderPatcherCreate, &patcherParams, &this->shaderPatcher);
	if (ret < 0) {
		return ret;
	}

	// render target
	SceGxmRenderTargetParams renderTargetParams;
	memset(&renderTargetParams, 0, sizeof(SceGxmRenderTargetParams));
	renderTargetParams.flags = 0;
	renderTargetParams.width = VITA_GXM_SCREEN_WIDTH;
	renderTargetParams.height = VITA_GXM_SCREEN_HEIGHT;
	renderTargetParams.scenesPerFrame = 1;
	renderTargetParams.multisampleMode = msaaMode;
	renderTargetParams.multisampleLocations = 0;
	renderTargetParams.driverMemBlock = -1; // Invalid UID
	ret = SCE_ERR(sceGxmCreateRenderTarget, &renderTargetParams, &this->renderTarget);
	if (ret < 0) {
		return ret;
	}

	for (int i = 0; i < GXM_DISPLAY_BUFFER_COUNT; i++) {
		this->displayBuffers[i] = vita_mem_alloc(
			SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
			4 * VITA_GXM_SCREEN_STRIDE * VITA_GXM_SCREEN_HEIGHT,
			SCE_GXM_COLOR_SURFACE_ALIGNMENT,
			SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
			&this->displayBuffersUid[i],
			"displayBuffers"
		);

		ret = SCE_ERR(
			sceGxmColorSurfaceInit,
			&this->displayBuffersSurface[i],
			SCE_GXM_COLOR_FORMAT_A8B8G8R8,
			SCE_GXM_COLOR_SURFACE_LINEAR,
			(msaaMode == SCE_GXM_MULTISAMPLE_NONE) ? SCE_GXM_COLOR_SURFACE_SCALE_NONE
												   : SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE,
			SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
			VITA_GXM_SCREEN_WIDTH,
			VITA_GXM_SCREEN_HEIGHT,
			VITA_GXM_SCREEN_STRIDE,
			this->displayBuffers[i]
		);
		if (ret < 0) {
			return ret;
		}

		ret = SCE_ERR(sceGxmSyncObjectCreate, &this->displayBuffersSync[i]);
		if (ret < 0) {
			return ret;
		}
	}

	// depth & stencil
	this->depthBufferData = vita_mem_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		4 * sampleCount,
		SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		&this->depthBufferUid,
		"depthBufferData"
	);

	this->stencilBufferData = vita_mem_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		1 * sampleCount,
		SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		&this->stencilBufferUid,
		"stencilBufferData"
	);

	ret = SCE_ERR(
		sceGxmDepthStencilSurfaceInit,
		&this->depthSurface,
		SCE_GXM_DEPTH_STENCIL_FORMAT_DF32_S8,
		SCE_GXM_DEPTH_STENCIL_SURFACE_TILED,
		depthStrideInSamples,
		this->depthBufferData,
		this->stencilBufferData
	);
	if (ret < 0) {
		return ret;
	}

	// allocator
	this->cdramMem = vita_mem_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
		CDRAM_POOL_SIZE,
		16,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		&this->cdramUID,
		"cdram_pool"
	);
	this->cdramPool = SDL_malloc(tlsf_size());
	tlsf_create(this->cdramPool);
	tlsf_add_pool(this->cdramPool, this->cdramMem, CDRAM_POOL_SIZE);

	// register plane, color, image shaders
	ret = SCE_ERR(
		sceGxmShaderPatcherRegisterProgram,
		this->shaderPatcher,
		planeVertexProgramGxp,
		&this->planeVertexProgramId
	);
	if (ret < 0) {
		return ret;
	}

	ret = SCE_ERR(
		sceGxmShaderPatcherRegisterProgram,
		this->shaderPatcher,
		colorFragmentProgramGxp,
		&this->colorFragmentProgramId
	);
	if (ret < 0) {
		return ret;
	}

	ret = SCE_ERR(
		sceGxmShaderPatcherRegisterProgram,
		this->shaderPatcher,
		imageFragmentProgramGxp,
		&this->imageFragmentProgramId
	);
	if (ret < 0) {
		return ret;
	}

	{
		GET_SHADER_PARAM(positionAttribute, planeVertexProgramGxp, "aPosition", -1);
		GET_SHADER_PARAM(texCoordAttribute, planeVertexProgramGxp, "aTexCoord", -1);

		SceGxmVertexAttribute vertexAttributes[2];
		SceGxmVertexStream vertexStreams[1];

		// position
		vertexAttributes[0].streamIndex = 0;
		vertexAttributes[0].offset = 0;
		vertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
		vertexAttributes[0].componentCount = 2;
		vertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(positionAttribute);

		// uv
		vertexAttributes[1].streamIndex = 0;
		vertexAttributes[1].offset = 8;
		vertexAttributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
		vertexAttributes[1].componentCount = 2;
		vertexAttributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(texCoordAttribute);

		vertexStreams[0].stride = sizeof(GXMVertex2D);
		vertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

		ret = SCE_ERR(
			sceGxmShaderPatcherCreateVertexProgram,
			this->shaderPatcher,
			this->planeVertexProgramId,
			vertexAttributes,
			2,
			vertexStreams,
			1,
			&this->planeVertexProgram
		);
		if (ret < 0) {
			return ret;
		}

		ret = SCE_ERR(
			sceGxmShaderPatcherCreateFragmentProgram,
			this->shaderPatcher,
			this->colorFragmentProgramId,
			SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
			SCE_GXM_MULTISAMPLE_NONE,
			NULL,
			planeVertexProgramGxp,
			&this->colorFragmentProgram
		);
		if (ret < 0) {
			return ret;
		}

		ret = SCE_ERR(
			sceGxmShaderPatcherCreateFragmentProgram,
			this->shaderPatcher,
			this->imageFragmentProgramId,
			SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
			SCE_GXM_MULTISAMPLE_NONE,
			&blendInfoTransparent,
			planeVertexProgramGxp,
			&this->imageFragmentProgram
		);
		if (ret < 0) {
			return ret;
		}
	}

	this->color_uColor = sceGxmProgramFindParameterByName(colorFragmentProgramGxp, "uColor"); // vec4

	this->clearVertices = static_cast<GXMVertex2D*>(this->alloc(sizeof(GXMVertex2D) * 4, 4));
	this->clearVertices[0] = {.position = {-1.0, 1.0}, .texCoord = {0, 0}};
	this->clearVertices[1] = {.position = {1.0, 1.0}, .texCoord = {0, 0}};
	this->clearVertices[2] = {.position = {-1.0, -1.0}, .texCoord = {0, 0}};
	this->clearVertices[3] = {.position = {1.0, -1.0}, .texCoord = {0, 0}};

	this->clearIndices = static_cast<uint16_t*>(this->alloc(sizeof(uint16_t) * 4, 4));
	this->clearIndices[0] = 0;
	this->clearIndices[1] = 1;
	this->clearIndices[2] = 2;
	this->clearIndices[3] = 3;

	return 0;
}

static int inuse_mem = 0;
void* GXMContext::alloc(size_t size, size_t align)
{
	DEBUG_ONLY_PRINTF("cdram_alloc(%d, %d) inuse=%d ", size, align, inuse_mem);
	void* ptr = tlsf_memalign(this->cdramPool, align, size);
	DEBUG_ONLY_PRINTF("ptr=%p\n", ptr);
	inuse_mem += tlsf_block_size(ptr);
	return ptr;
}

void GXMContext::free(void* ptr)
{
	inuse_mem -= tlsf_block_size(ptr);
	DEBUG_ONLY_PRINTF("cdram_free(%p)\n", ptr);
	tlsf_free(this->cdramPool, ptr);
}

void GXMContext::clear(float r, float g, float b, bool new_scene)
{
	new_scene = new_scene && !this->sceneStarted;
	if (new_scene) {
		sceGxmBeginScene(
			this->context,
			0,
			this->renderTarget,
			nullptr,
			nullptr,
			this->displayBuffersSync[this->backBufferIndex],
			&this->displayBuffersSurface[this->backBufferIndex],
			&this->depthSurface
		);
		this->sceneStarted = true;
	}

	float color[] = {r, g, b, 1};

	sceGxmSetVertexProgram(this->context, this->planeVertexProgram);
	sceGxmSetFragmentProgram(this->context, this->colorFragmentProgram);

	void* vertUniforms;
	void* fragUniforms;
	sceGxmReserveVertexDefaultUniformBuffer(gxm->context, &vertUniforms);
	sceGxmReserveFragmentDefaultUniformBuffer(gxm->context, &fragUniforms);

	sceGxmSetVertexStream(gxm->context, 0, this->clearVertices);
	sceGxmSetUniformDataF(fragUniforms, this->color_uColor, 0, 4, color);

	sceGxmSetFrontDepthFunc(gxm->context, SCE_GXM_DEPTH_FUNC_ALWAYS);
	sceGxmDraw(gxm->context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, this->clearIndices, 4);
	if (new_scene) {
		sceGxmEndScene(this->context, nullptr, nullptr);
		this->sceneStarted = false;
	}
}

void GXMContext::copy_frontbuffer()
{
	SceGxmTexture texture;
	sceGxmTextureInitLinearStrided(
		&texture,
		this->displayBuffers[this->frontBufferIndex],
		SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR,
		VITA_GXM_SCREEN_WIDTH,
		VITA_GXM_SCREEN_HEIGHT,
		VITA_GXM_SCREEN_STRIDE * 4
	);
	sceGxmSetVertexProgram(this->context, this->planeVertexProgram);
	sceGxmSetFragmentProgram(this->context, this->imageFragmentProgram);

	void* vertUniforms;
	void* fragUniforms;
	sceGxmReserveVertexDefaultUniformBuffer(this->context, &vertUniforms);
	sceGxmReserveFragmentDefaultUniformBuffer(this->context, &fragUniforms);

	sceGxmSetVertexStream(this->context, 0, this->clearVertices);
	sceGxmSetFragmentTexture(this->context, 0, &texture);

	sceGxmSetFrontDepthFunc(this->context, SCE_GXM_DEPTH_FUNC_ALWAYS);
	sceGxmDraw(this->context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, this->clearIndices, 4);
}

void GXMContext::destroy()
{
	sceGxmDisplayQueueFinish();
	if (gxm->context) {
		sceGxmFinish(gxm->context);
	}
	if (this->renderTarget) {
		sceGxmDestroyRenderTarget(this->renderTarget);
	}
	for (int i = 0; i < GXM_DISPLAY_BUFFER_COUNT; i++) {
		if (this->displayBuffersUid[i]) {
			vita_mem_free(this->displayBuffersUid[i]);
			this->displayBuffers[i] = nullptr;
			sceGxmSyncObjectDestroy(this->displayBuffersSync[i]);
		}
	}

	if (this->depthBufferUid) {
		vita_mem_free(this->depthBufferUid);
	}
	if (this->stencilBufferUid) {
		vita_mem_free(this->stencilBufferUid);
	}
	this->stencilBufferData = nullptr;
	this->depthBufferData = nullptr;

	sceGxmShaderPatcherDestroy(this->shaderPatcher);
	sceGxmDestroyContext(this->context);
	vita_mem_fragment_usse_free(this->fragmentUsseRingBufferUid);
	vita_mem_free(this->fragmentRingBufferUid);
	vita_mem_free(this->vertexRingBufferUid);
	vita_mem_free(this->vdmRingBufferUid);
	SDL_free(this->contextHostMem);
}

void GXMContext::swap_display()
{
	if (this->sceneStarted) {
		sceGxmEndScene(gxm->context, nullptr, nullptr);
		this->sceneStarted = false;
	}

	SceCommonDialogUpdateParam updateParam;
	SDL_zero(updateParam);
	updateParam.renderTarget.colorFormat = VITA_GXM_COLOR_FORMAT;
	updateParam.renderTarget.surfaceType = SCE_GXM_COLOR_SURFACE_LINEAR;
	updateParam.renderTarget.width = VITA_GXM_SCREEN_WIDTH;
	updateParam.renderTarget.height = VITA_GXM_SCREEN_HEIGHT;
	updateParam.renderTarget.strideInPixels = VITA_GXM_SCREEN_STRIDE;
	updateParam.renderTarget.colorSurfaceData = this->displayBuffers[this->backBufferIndex];
	updateParam.displaySyncObject = this->displayBuffersSync[this->backBufferIndex];
	sceCommonDialogUpdate(&updateParam);

	sceGxmPadHeartbeat(
		&this->displayBuffersSurface[this->backBufferIndex],
		this->displayBuffersSync[this->backBufferIndex]
	);

	// display
	GXMDisplayData displayData;
	displayData.address = this->displayBuffers[this->backBufferIndex];
	displayData.index = this->backBufferIndex;
	sceGxmDisplayQueueAddEntry(
		this->displayBuffersSync[this->frontBufferIndex],
		this->displayBuffersSync[this->backBufferIndex],
		&displayData
	);

	this->frontBufferIndex = this->backBufferIndex;
	this->backBufferIndex = (this->backBufferIndex + 1) % GXM_DISPLAY_BUFFER_COUNT;
}

Direct3DRMRenderer* GXMRenderer::Create(DWORD width, DWORD height)
{
	int ret = gxm_library_init();
	if (ret < 0) {
		return nullptr;
	}
	return new GXMRenderer(width, height);
}

GXMRenderer::GXMRenderer(DWORD width, DWORD height)
{
	m_width = VITA_GXM_SCREEN_WIDTH;
	m_height = VITA_GXM_SCREEN_HEIGHT;
	m_virtualWidth = width;
	m_virtualHeight = height;

	int ret;
	if (!gxm) {
		gxm = (GXMContext*) SDL_malloc(sizeof(GXMContext));
	}
	ret = SCE_ERR(gxm->init);
	if (ret < 0) {
		return;
	}

	// register shader programs
	ret = SCE_ERR(
		sceGxmShaderPatcherRegisterProgram,
		gxm->shaderPatcher,
		mainVertexProgramGxp,
		&this->mainVertexProgramId
	);
	if (ret < 0) {
		return;
	}

	ret = SCE_ERR(
		sceGxmShaderPatcherRegisterProgram,
		gxm->shaderPatcher,
		mainColorFragmentProgramGxp,
		&this->mainColorFragmentProgramId
	);
	if (ret < 0) {
		return;
	}

	ret = SCE_ERR(
		sceGxmShaderPatcherRegisterProgram,
		gxm->shaderPatcher,
		mainTextureFragmentProgramGxp,
		&this->mainTextureFragmentProgramId
	);
	if (ret < 0) {
		return;
	}

	// main shader
	{
		GET_SHADER_PARAM(positionAttribute, mainVertexProgramGxp, "aPosition", );
		GET_SHADER_PARAM(normalAttribute, mainVertexProgramGxp, "aNormal", );
		GET_SHADER_PARAM(texCoordAttribute, mainVertexProgramGxp, "aTexCoord", );

		SceGxmVertexAttribute vertexAttributes[3];
		SceGxmVertexStream vertexStreams[1];

		// position
		vertexAttributes[0].streamIndex = 0;
		vertexAttributes[0].offset = 0;
		vertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
		vertexAttributes[0].componentCount = 3;
		vertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(positionAttribute);

		// normal
		vertexAttributes[1].streamIndex = 0;
		vertexAttributes[1].offset = 12;
		vertexAttributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
		vertexAttributes[1].componentCount = 3;
		vertexAttributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(normalAttribute);

		vertexAttributes[2].streamIndex = 0;
		vertexAttributes[2].offset = 24;
		vertexAttributes[2].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
		vertexAttributes[2].componentCount = 2;
		vertexAttributes[2].regIndex = sceGxmProgramParameterGetResourceIndex(texCoordAttribute);

		vertexStreams[0].stride = sizeof(GXMVertex);
		vertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

		ret = SCE_ERR(
			sceGxmShaderPatcherCreateVertexProgram,
			gxm->shaderPatcher,
			this->mainVertexProgramId,
			vertexAttributes,
			3,
			vertexStreams,
			1,
			&this->mainVertexProgram
		);
		if (ret < 0) {
			return;
		}
	}

	// main color opaque
	ret = SCE_ERR(
		sceGxmShaderPatcherCreateFragmentProgram,
		gxm->shaderPatcher,
		this->mainColorFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaaMode,
		&blendInfoOpaque,
		mainVertexProgramGxp,
		&this->opaqueColorFragmentProgram
	);
	if (ret < 0) {
		return;
	}

	// main color blended
	ret = SCE_ERR(
		sceGxmShaderPatcherCreateFragmentProgram,
		gxm->shaderPatcher,
		this->mainColorFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaaMode,
		&blendInfoTransparent,
		mainVertexProgramGxp,
		&this->blendedColorFragmentProgram
	);
	if (ret < 0) {
		return;
	}

	// main texture opaque
	ret = SCE_ERR(
		sceGxmShaderPatcherCreateFragmentProgram,
		gxm->shaderPatcher,
		this->mainTextureFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaaMode,
		&blendInfoOpaque,
		mainVertexProgramGxp,
		&this->opaqueTextureFragmentProgram
	);
	if (ret < 0) {
		return;
	}

	// main texture transparent
	ret = SCE_ERR(
		sceGxmShaderPatcherCreateFragmentProgram,
		gxm->shaderPatcher,
		this->mainTextureFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaaMode,
		&blendInfoTransparent,
		mainVertexProgramGxp,
		&this->blendedTextureFragmentProgram
	);
	if (ret < 0) {
		return;
	}

	// vertex uniforms
	this->uModelViewMatrix = sceGxmProgramFindParameterByName(mainVertexProgramGxp, "uModelViewMatrix");
	this->uNormalMatrix = sceGxmProgramFindParameterByName(mainVertexProgramGxp, "uNormalMatrix");
	this->uProjectionMatrix = sceGxmProgramFindParameterByName(mainVertexProgramGxp, "uProjectionMatrix");

	// fragment uniforms
	this->uLights = sceGxmProgramFindParameterByName(mainColorFragmentProgramGxp, "uLights"); // SceneLight[2]
	this->uAmbientLight = sceGxmProgramFindParameterByName(mainColorFragmentProgramGxp, "uAmbientLight"); // vec3
	this->uShininess = sceGxmProgramFindParameterByName(mainColorFragmentProgramGxp, "uShininess");       // float
	this->uColor = sceGxmProgramFindParameterByName(mainColorFragmentProgramGxp, "uColor");               // vec4

	for (int i = 0; i < GXM_FRAGMENT_BUFFER_COUNT; i++) {
		this->lights[i] = static_cast<GXMSceneLightUniform*>(gxm->alloc(sizeof(GXMSceneLightUniform), 4));
	}
	for (int i = 0; i < GXM_VERTEX_BUFFER_COUNT; i++) {
		this->quadVertices[i] = static_cast<GXMVertex2D*>(gxm->alloc(sizeof(GXMVertex2D) * 4 * 50, 4));
	}
	this->quadIndices = static_cast<uint16_t*>(gxm->alloc(sizeof(uint16_t) * 4, 4));
	this->quadIndices[0] = 0;
	this->quadIndices[1] = 1;
	this->quadIndices[2] = 2;
	this->quadIndices[3] = 3;

	volatile uint32_t* notificationMem = sceGxmGetNotificationRegion();
	for (uint32_t i = 0; i < GXM_FRAGMENT_BUFFER_COUNT; i++) {
		this->fragmentNotifications[i].address = notificationMem++;
		this->fragmentNotifications[i].value = 0;
	}
	this->currentFragmentBufferIndex = 0;

	for (uint32_t i = 0; i < GXM_VERTEX_BUFFER_COUNT; i++) {
		this->vertexNotifications[i].address = notificationMem++;
		this->vertexNotifications[i].value = 0;
	}
	this->currentVertexBufferIndex = 0;
	m_initialized = true;
}

GXMRenderer::~GXMRenderer()
{
	for (int i = 0; i < GXM_FRAGMENT_BUFFER_COUNT; i++) {
		if (this->lights[i]) {
			gxm->free(this->lights[i]);
		}
	}
	for (int i = 0; i < GXM_VERTEX_BUFFER_COUNT; i++) {
		if (this->quadVertices[i]) {
			gxm->free(this->quadVertices[i]);
		}
	}
	if (this->quadIndices) {
		gxm->free(this->quadIndices);
	}
}

void GXMRenderer::PushLights(const SceneLight* lightsArray, size_t count)
{
	if (count > 3) {
		SDL_Log("Unsupported number of lights (%d)", static_cast<int>(count));
		count = 3;
	}

	m_lights.assign(lightsArray, lightsArray + count);
}

void GXMRenderer::SetFrustumPlanes(const Plane* frustumPlanes)
{
}

void GXMRenderer::SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back)
{
	memcpy(&m_projection, projection, sizeof(D3DRMMATRIX4D));
}

struct TextureDestroyContextGXM {
	GXMRenderer* renderer;
	Uint32 textureId;
};

void GXMRenderer::AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture)
{
	auto* ctx = new TextureDestroyContextGXM{this, id};
	texture->AddDestroyCallback(
		[](IDirect3DRMObject* obj, void* arg) {
			auto* ctx = static_cast<TextureDestroyContextGXM*>(arg);
			auto& cache = ctx->renderer->m_textures[ctx->textureId];
			ctx->renderer->m_textures_delete[gxm->backBufferIndex].push_back(cache.gxmTexture);
			cache.texture = nullptr;
			memset(&cache.gxmTexture, 0, sizeof(SceGxmTexture));
			delete ctx;
		},
		ctx
	);
}

void GXMRenderer::DeleteTextures(int index)
{
	for (auto& del : this->m_textures_delete[index]) {
		void* textureData = sceGxmTextureGetData(&del);
		gxm->free(textureData);
	}
	this->m_textures_delete[index].clear();
}

static void convertTextureMetadata(
	SDL_Surface* surface,
	bool* supportedFormat,
	SceGxmTextureFormat* gxmTextureFormat,
	size_t* textureSize,      // size in bytes
	size_t* textureAlignment, // alignment in bytes
	size_t* textureStride,    // stride in bytes
	size_t* paletteOffset     // offset from textureData in bytes
)
{
	int bytesPerPixel;
	size_t extraDataSize = 0;
	switch (surface->format) {
	case SDL_PIXELFORMAT_INDEX8: {
		*supportedFormat = true;
		*gxmTextureFormat = SCE_GXM_TEXTURE_FORMAT_P8_ABGR;
		int pixelsSize = surface->w * surface->h;
		int alignBytes = ALIGNMENT(pixelsSize, SCE_GXM_PALETTE_ALIGNMENT);
		extraDataSize = alignBytes + 256 * 4;
		*textureAlignment = SCE_GXM_PALETTE_ALIGNMENT;
		*paletteOffset = pixelsSize + alignBytes;
		bytesPerPixel = 1;
		break;
	}
	case SDL_PIXELFORMAT_ABGR8888: {
		*supportedFormat = true;
		*gxmTextureFormat = SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR;
		*textureAlignment = SCE_GXM_TEXTURE_ALIGNMENT;
		bytesPerPixel = 4;
		break;
	}
	default: {
		*supportedFormat = false;
		*gxmTextureFormat = SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR;
		*textureAlignment = SCE_GXM_TEXTURE_ALIGNMENT;
		bytesPerPixel = 4;
		break;
	}
	}
	*textureStride = ALIGN(surface->w, 8) * bytesPerPixel;
	*textureSize = (*textureStride) * surface->h + extraDataSize;
}

void copySurfaceToGxm(DirectDrawSurfaceImpl* surface, uint8_t* textureData, size_t dstStride, size_t dstSize)
{
	SDL_Surface* src = surface->m_surface;
	switch (src->format) {
	case SDL_PIXELFORMAT_ABGR8888: {
		for (int y = 0; y < src->h; y++) {
			uint8_t* srcRow = (uint8_t*) src->pixels + (y * src->pitch);
			uint8_t* dstRow = textureData + (y * dstStride);
			size_t rowSize = src->w * 4;
			memcpy(dstRow, srcRow, rowSize);
		}
		break;
	}
	case SDL_PIXELFORMAT_INDEX8: {
		LPDIRECTDRAWPALETTE _palette;
		surface->GetPalette(&_palette);
		auto palette = static_cast<DirectDrawPaletteImpl*>(_palette);

		// copy pixels
		for (int y = 0; y < src->h; y++) {
			void* srcRow = static_cast<uint8_t*>(src->pixels) + (y * src->pitch);
			void* dstRow = static_cast<uint8_t*>(textureData) + (y * dstStride);
			memcpy(dstRow, srcRow, src->w);
		}

		int pixelsSize = src->w * src->h;
		int alignBytes = ALIGNMENT(pixelsSize, SCE_GXM_PALETTE_ALIGNMENT);
		uint8_t* paletteData = textureData + pixelsSize + alignBytes;
		memcpy(paletteData, palette->m_palette->colors, 256 * 4);
		palette->Release();
		break;
	}
	default: {
		DEBUG_ONLY_PRINTF("unsupported format %d\n", SDL_GetPixelFormatName(src->format));
		SDL_Surface* dst = SDL_CreateSurfaceFrom(src->w, src->h, SDL_PIXELFORMAT_ABGR8888, textureData, src->w * 4);
		SDL_BlitSurface(src, nullptr, dst, nullptr);
		SDL_DestroySurface(dst);
		break;
	}
	}
}

Uint32 GXMRenderer::GetTextureId(IDirect3DRMTexture* iTexture, bool isUi, float scaleX, float scaleY)
{
	auto texture = static_cast<Direct3DRMTextureImpl*>(iTexture);
	auto surface = static_cast<DirectDrawSurfaceImpl*>(texture->m_surface);

	bool supportedFormat;
	SceGxmTextureFormat gxmTextureFormat;
	size_t textureSize;
	size_t textureAlignment;
	size_t textureStride;
	size_t paletteOffset;

	int textureWidth = surface->m_surface->w;
	int textureHeight = surface->m_surface->h;

	convertTextureMetadata(
		surface->m_surface,
		&supportedFormat,
		&gxmTextureFormat,
		&textureSize,
		&textureAlignment,
		&textureStride,
		&paletteOffset
	);

	if (!supportedFormat) {
		return NO_TEXTURE_ID;
	}

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (tex.texture == texture) {
			if (tex.version != texture->m_version) {
				sceGxmNotificationWait(tex.notification);
				tex.notification = &this->fragmentNotifications[this->currentFragmentBufferIndex];
				uint8_t* textureData = (uint8_t*) sceGxmTextureGetData(&tex.gxmTexture);
				copySurfaceToGxm(surface, textureData, textureStride, textureSize);
				tex.version = texture->m_version;
			}
			return i;
		}
	}

	DEBUG_ONLY_PRINTF(
		"Create Texture %s w=%d h=%d s=%d size=%d align=%d\n",
		SDL_GetPixelFormatName(surface->m_surface->format),
		textureWidth,
		textureHeight,
		textureStride,
		textureSize,
		textureAlignment
	);

	// allocate gpu memory
	void* textureData = gxm->alloc(textureSize, textureAlignment);
	copySurfaceToGxm(surface, (uint8_t*) textureData, textureStride, textureSize);

	SceGxmTexture gxmTexture;
	SCE_ERR(sceGxmTextureInitLinear, &gxmTexture, textureData, gxmTextureFormat, textureWidth, textureHeight, 0);
	if (isUi) {
		sceGxmTextureSetMinFilter(&gxmTexture, SCE_GXM_TEXTURE_FILTER_POINT);
		sceGxmTextureSetMagFilter(&gxmTexture, SCE_GXM_TEXTURE_FILTER_POINT);
		sceGxmTextureSetUAddrMode(&gxmTexture, SCE_GXM_TEXTURE_ADDR_CLAMP);
		sceGxmTextureSetVAddrMode(&gxmTexture, SCE_GXM_TEXTURE_ADDR_CLAMP);
	}
	else {
		sceGxmTextureSetMinFilter(&gxmTexture, SCE_GXM_TEXTURE_FILTER_LINEAR);
		sceGxmTextureSetMagFilter(&gxmTexture, SCE_GXM_TEXTURE_FILTER_LINEAR);
		sceGxmTextureSetUAddrMode(&gxmTexture, SCE_GXM_TEXTURE_ADDR_REPEAT);
		sceGxmTextureSetVAddrMode(&gxmTexture, SCE_GXM_TEXTURE_ADDR_REPEAT);
	}
	if (gxmTextureFormat == SCE_GXM_TEXTURE_FORMAT_P8_ABGR) {
		sceGxmTextureSetPalette(&gxmTexture, (uint8_t*) textureData + paletteOffset);
	}

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (!tex.texture) {
			memset(&tex, 0, sizeof(tex));
			tex.texture = texture;
			tex.version = texture->m_version;
			tex.gxmTexture = gxmTexture;
			tex.notification = &this->fragmentNotifications[this->currentFragmentBufferIndex];
			AddTextureDestroyCallback(i, texture);
			return i;
		}
	}

	GXMTextureCacheEntry tex;
	memset(&tex, 0, sizeof(tex));
	tex.texture = texture;
	tex.version = texture->m_version;
	tex.gxmTexture = gxmTexture;
	tex.notification = &this->fragmentNotifications[this->currentFragmentBufferIndex];
	m_textures.push_back(tex);
	Uint32 textureId = (Uint32) (m_textures.size() - 1);
	AddTextureDestroyCallback(textureId, texture);
	return textureId;
}

const SceGxmTexture* GXMRenderer::UseTexture(GXMTextureCacheEntry& texture)
{
	texture.notification = &this->fragmentNotifications[this->currentFragmentBufferIndex];
	sceGxmSetFragmentTexture(gxm->context, 0, &texture.gxmTexture);
	return &texture.gxmTexture;
}

GXMMeshCacheEntry GXMRenderer::GXMUploadMesh(const MeshGroup& meshGroup)
{
	GXMMeshCacheEntry cache{&meshGroup, meshGroup.version};

	cache.flat = meshGroup.quality == D3DRMRENDER_FLAT || meshGroup.quality == D3DRMRENDER_UNLITFLAT;

	std::vector<D3DRMVERTEX> vertices;
	std::vector<uint16_t> indices;
	if (cache.flat) {
		FlattenSurfaces(
			meshGroup.vertices.data(),
			meshGroup.vertices.size(),
			meshGroup.indices.data(),
			meshGroup.indices.size(),
			meshGroup.texture != nullptr,
			vertices,
			indices
		);
	}
	else {
		vertices = meshGroup.vertices;
		indices.resize(meshGroup.indices.size());
		std::transform(meshGroup.indices.begin(), meshGroup.indices.end(), indices.begin(), [](DWORD index) {
			return static_cast<uint16_t>(index);
		});
	}

	size_t vertexBufferSize = sizeof(GXMVertex) * vertices.size();
	size_t indexBufferSize = sizeof(uint16_t) * indices.size();
	void* meshData = gxm->alloc(vertexBufferSize + indexBufferSize, 4);

	GXMVertex* vertexBuffer = (GXMVertex*) meshData;
	uint16_t* indexBuffer = (uint16_t*) ((uint8_t*) meshData + vertexBufferSize);

	for (int i = 0; i < vertices.size(); i++) {
		D3DRMVERTEX vertex = vertices.data()[i];
		vertexBuffer[i] = GXMVertex{
			.position =
				{
					vertex.position.x,
					vertex.position.y,
					vertex.position.z,
				},
			.normal =
				{
					vertex.normal.x,
					vertex.normal.y,
					vertex.normal.z,
				},
			.texCoord =
				{
					vertex.tu,
					vertex.tv,
				}
		};
	}
	memcpy(indexBuffer, indices.data(), indices.size() * sizeof(uint16_t));

	cache.meshData = meshData;
	cache.vertexBuffer = vertexBuffer;
	cache.indexBuffer = indexBuffer;
	cache.indexCount = indices.size();
	return cache;
}

struct GXMMeshDestroyContext {
	GXMRenderer* renderer;
	Uint32 id;
};

void GXMRenderer::AddMeshDestroyCallback(Uint32 id, IDirect3DRMMesh* mesh)
{
	auto* ctx = new GXMMeshDestroyContext{this, id};
	mesh->AddDestroyCallback(
		[](IDirect3DRMObject*, void* arg) {
			auto* ctx = static_cast<GXMMeshDestroyContext*>(arg);
			auto& cache = ctx->renderer->m_meshes[ctx->id];
			cache.meshGroup = nullptr;
			gxm->free(cache.meshData);
			cache.meshData = nullptr;
			cache.indexBuffer = nullptr;
			cache.vertexBuffer = nullptr;
			cache.indexCount = 0;
			delete ctx;
		},
		ctx
	);
}

Uint32 GXMRenderer::GetMeshId(IDirect3DRMMesh* mesh, const MeshGroup* meshGroup)
{
	for (Uint32 i = 0; i < m_meshes.size(); ++i) {
		auto& cache = m_meshes[i];
		if (cache.meshGroup == meshGroup) {
			if (cache.version != meshGroup->version) {
				cache = std::move(this->GXMUploadMesh(*meshGroup));
			}
			return i;
		}
	}

	auto newCache = this->GXMUploadMesh(*meshGroup);

	for (Uint32 i = 0; i < m_meshes.size(); ++i) {
		auto& cache = m_meshes[i];
		if (!cache.meshGroup) {
			cache = std::move(newCache);
			AddMeshDestroyCallback(i, mesh);
			return i;
		}
	}

	m_meshes.push_back(std::move(newCache));
	AddMeshDestroyCallback((Uint32) (m_meshes.size() - 1), mesh);
	return (Uint32) (m_meshes.size() - 1);
}

bool razor_live_started = false;
bool razor_display_enabled = false;

void GXMRenderer::StartScene()
{
	if (gxm->sceneStarted) {
		return;
	}

#ifdef GXM_WITH_RAZOR
	bool dpad_up_clicked = !this->last_dpad_up && g_dpadUp;
	bool dpad_down_clicked = !this->last_dpad_down && g_dpadDown;
	bool dpad_left_clicked = !this->last_dpad_left && g_dpadLeft;
	bool dpad_right_clicked = !this->last_dpad_right && g_dpadRight;
	this->last_dpad_up = g_dpadUp;
	this->last_dpad_down = g_dpadDown;
	this->last_dpad_left = g_dpadLeft;
	this->last_dpad_right = g_dpadRight;

	if (with_razor_hud) {
		if (dpad_up_clicked) {
			razor_display_enabled = !razor_display_enabled;
			sceRazorHudSetDisplayEnabled(razor_display_enabled);
		}
		if (dpad_left_clicked) {
			if (razor_live_started) {
				sceRazorGpuLiveStop();
			}
			else {
				sceRazorGpuLiveStart();
			}
			razor_live_started = !razor_live_started;
		}
		if (dpad_right_clicked) {
			sceRazorGpuTraceTrigger();
		}
	}
	if (with_razor_capture) {
		if (dpad_down_clicked) {
			sceRazorGpuCaptureSetTriggerNextFrame("ux0:/data/capture.sgx");
		}
	}
#endif

	sceGxmBeginScene(
		gxm->context,
		0,
		gxm->renderTarget,
		nullptr,
		nullptr,
		gxm->displayBuffersSync[gxm->backBufferIndex],
		&gxm->displayBuffersSurface[gxm->backBufferIndex],
		&gxm->depthSurface
	);
	sceGxmSetCullMode(gxm->context, SCE_GXM_CULL_CCW);
	gxm->sceneStarted = true;
	this->quadsUsed = 0;
	this->cleared = false;

	sceGxmNotificationWait(&this->vertexNotifications[this->currentVertexBufferIndex]);
	sceGxmNotificationWait(&this->fragmentNotifications[this->currentFragmentBufferIndex]);
}

HRESULT GXMRenderer::BeginFrame()
{
	this->transparencyEnabled = false;
	this->StartScene();

	auto lightData = this->LightsBuffer();
	int i = 0;
	for (const auto& light : m_lights) {
		if (!light.directional && !light.positional) {
			lightData->ambientLight[0] = light.color.r;
			lightData->ambientLight[1] = light.color.g;
			lightData->ambientLight[2] = light.color.b;
			continue;
		}
		if (i == 2) {
			sceClibPrintf("light overflow\n");
			continue;
		}

		lightData->lights[i].color[0] = light.color.r;
		lightData->lights[i].color[1] = light.color.g;
		lightData->lights[i].color[2] = light.color.b;
		lightData->lights[i].color[3] = light.color.a;

		bool isDirectional = light.directional == 1.0;
		if (isDirectional) {
			lightData->lights[i].vec[0] = light.direction.x;
			lightData->lights[i].vec[1] = light.direction.y;
			lightData->lights[i].vec[2] = light.direction.z;
		}
		else {
			lightData->lights[i].vec[0] = light.position.x;
			lightData->lights[i].vec[1] = light.position.y;
			lightData->lights[i].vec[2] = light.position.z;
		}
		lightData->lights[i].isDirectional = isDirectional;
		i++;
	}
	sceGxmSetFragmentUniformBuffer(gxm->context, 0, lightData);

	return DD_OK;
}

void GXMRenderer::EnableTransparency()
{
	this->transparencyEnabled = true;
}

void GXMRenderer::SubmitDraw(
	DWORD meshId,
	const D3DRMMATRIX4D& modelViewMatrix,
	const D3DRMMATRIX4D& worldMatrix,
	const D3DRMMATRIX4D& viewMatrix,
	const Matrix3x3& normalMatrix,
	const Appearance& appearance
)
{
	auto& mesh = m_meshes[meshId];

#ifdef DEBUG
	char marker[256];
	snprintf(marker, sizeof(marker), "SubmitDraw: %d", meshId);
	sceGxmPushUserMarker(gxm->context, marker);
#endif

	bool textured = appearance.textureId != NO_TEXTURE_ID;
	const SceGxmFragmentProgram* fragmentProgram;
	if (this->transparencyEnabled) {
		fragmentProgram = textured ? this->blendedTextureFragmentProgram : this->blendedColorFragmentProgram;
	}
	else {
		fragmentProgram = textured ? this->opaqueTextureFragmentProgram : this->opaqueColorFragmentProgram;
	}
	sceGxmSetVertexProgram(gxm->context, this->mainVertexProgram);
	sceGxmSetFragmentProgram(gxm->context, fragmentProgram);

	void* vertUniforms;
	void* fragUniforms;
	sceGxmReserveVertexDefaultUniformBuffer(gxm->context, &vertUniforms);
	sceGxmReserveFragmentDefaultUniformBuffer(gxm->context, &fragUniforms);

	// vertex uniforms
	sceGxmSetUniformDataF(vertUniforms, this->uModelViewMatrix, 0, 4 * 4, &modelViewMatrix[0][0]);
	sceGxmSetUniformDataF(vertUniforms, this->uNormalMatrix, 0, 3 * 3, &normalMatrix[0][0]);
	sceGxmSetUniformDataF(vertUniforms, this->uProjectionMatrix, 0, 4 * 4, &this->m_projection[0][0]);

	// fragment uniforms
	float color[4] = {
		appearance.color.r / 255.0f,
		appearance.color.g / 255.0f,
		appearance.color.b / 255.0f,
		appearance.color.a / 255.0f
	};
	sceGxmSetUniformDataF(fragUniforms, this->uColor, 0, 4, color);
	sceGxmSetUniformDataF(fragUniforms, this->uShininess, 0, 1, &appearance.shininess);

	if (textured) {
		auto& texture = m_textures[appearance.textureId];
		this->UseTexture(texture);
	}
	sceGxmSetVertexStream(gxm->context, 0, mesh.vertexBuffer);
	sceGxmSetFrontDepthFunc(gxm->context, SCE_GXM_DEPTH_FUNC_LESS_EQUAL);
	sceGxmDraw(gxm->context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, mesh.indexBuffer, mesh.indexCount);

#ifdef DEBUG
	sceGxmPopUserMarker(gxm->context);
#endif
}

HRESULT GXMRenderer::FinalizeFrame()
{
	return DD_OK;
}

void GXMRenderer::Resize(int width, int height, const ViewportTransform& viewportTransform)
{
	m_width = width;
	m_height = height;
	m_viewportTransform = viewportTransform;
}

void GXMRenderer::Clear(float r, float g, float b)
{
	this->StartScene();
	gxm->clear(r, g, b, false);
	this->cleared = true;
}

void GXMRenderer::Flip()
{
	if (!gxm->sceneStarted) {
		return;
	}

	++this->vertexNotifications[this->currentVertexBufferIndex].value;
	++this->fragmentNotifications[this->currentFragmentBufferIndex].value;
	sceGxmEndScene(
		gxm->context,
		&this->vertexNotifications[this->currentVertexBufferIndex],
		&this->fragmentNotifications[this->currentFragmentBufferIndex]
	);
	gxm->sceneStarted = false;

	this->currentVertexBufferIndex = (this->currentVertexBufferIndex + 1) % GXM_VERTEX_BUFFER_COUNT;
	this->currentFragmentBufferIndex = (this->currentFragmentBufferIndex + 1) % GXM_FRAGMENT_BUFFER_COUNT;
	gxm->swap_display();
}

void GXMRenderer::Draw2DImage(Uint32 textureId, const SDL_Rect& srcRect, const SDL_Rect& dstRect, FColor color)
{
	this->StartScene();
	if (!this->cleared) {
		gxm->clear(0, 0, 0, false);
		this->cleared = true;
	}

#ifdef DEBUG
	char marker[256];
	snprintf(marker, sizeof(marker), "Draw2DImage: %d", textureId);
	sceGxmPushUserMarker(gxm->context, marker);
#endif

	sceGxmSetVertexProgram(gxm->context, gxm->planeVertexProgram);
	if (textureId != NO_TEXTURE_ID) {
		sceGxmSetFragmentProgram(gxm->context, gxm->imageFragmentProgram);
	}
	else {
		sceGxmSetFragmentProgram(gxm->context, gxm->colorFragmentProgram);
	}

	void* vertUniforms;
	void* fragUniforms;
	sceGxmReserveVertexDefaultUniformBuffer(gxm->context, &vertUniforms);
	sceGxmReserveFragmentDefaultUniformBuffer(gxm->context, &fragUniforms);

	float left = -this->m_viewportTransform.offsetX / this->m_viewportTransform.scale;
	float right = (this->m_width - this->m_viewportTransform.offsetX) / this->m_viewportTransform.scale;
	float top = -this->m_viewportTransform.offsetY / this->m_viewportTransform.scale;
	float bottom = (this->m_height - this->m_viewportTransform.offsetY) / this->m_viewportTransform.scale;

#define virtualToNDCX(x) (((x - left) / (right - left)) * 2 - 1);
#define virtualToNDCY(y) -(((y - top) / (bottom - top)) * 2 - 1);

	float x1_virtual = static_cast<float>(dstRect.x);
	float y1_virtual = static_cast<float>(dstRect.y);
	float x2_virtual = x1_virtual + dstRect.w;
	float y2_virtual = y1_virtual + dstRect.h;

	float x1 = virtualToNDCX(x1_virtual);
	float y1 = virtualToNDCY(y1_virtual);
	float x2 = virtualToNDCX(x2_virtual);
	float y2 = virtualToNDCY(y2_virtual);

	float u1 = 0.0;
	float v1 = 0.0;
	float u2 = 0.0;
	float v2 = 0.0;

	if (textureId != NO_TEXTURE_ID) {
		GXMTextureCacheEntry& texture = m_textures[textureId];
		const SceGxmTexture* gxmTexture = this->UseTexture(texture);
		float texW = sceGxmTextureGetWidth(gxmTexture);
		float texH = sceGxmTextureGetHeight(gxmTexture);

		u1 = static_cast<float>(srcRect.x) / texW;
		v1 = static_cast<float>(srcRect.y) / texH;
		u2 = static_cast<float>(srcRect.x + srcRect.w) / texW;
		v2 = static_cast<float>(srcRect.y + srcRect.h) / texH;
	}
	else {
		SET_UNIFORM(fragUniforms, gxm->color_uColor, color);
	}

	GXMVertex2D* quadVertices = this->QuadVerticesBuffer();
	quadVertices[0] = GXMVertex2D{.position = {x1, y1}, .texCoord = {u1, v1}};
	quadVertices[1] = GXMVertex2D{.position = {x2, y1}, .texCoord = {u2, v1}};
	quadVertices[2] = GXMVertex2D{.position = {x1, y2}, .texCoord = {u1, v2}};
	quadVertices[3] = GXMVertex2D{.position = {x2, y2}, .texCoord = {u2, v2}};

	sceGxmSetVertexStream(gxm->context, 0, quadVertices);

	sceGxmSetFrontDepthWriteEnable(gxm->context, SCE_GXM_DEPTH_WRITE_DISABLED);
	sceGxmSetFrontDepthFunc(gxm->context, SCE_GXM_DEPTH_FUNC_ALWAYS);
	sceGxmDraw(gxm->context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, this->quadIndices, 4);
	sceGxmSetFrontDepthWriteEnable(gxm->context, SCE_GXM_DEPTH_WRITE_ENABLED);

#ifdef DEBUG
	sceGxmPopUserMarker(gxm->context);
#endif
}

void GXMRenderer::SetDither(bool dither)
{
}

void GXMRenderer::Download(SDL_Surface* target)
{
	SDL_Rect srcRect = {
		static_cast<int>(m_viewportTransform.offsetX),
		static_cast<int>(m_viewportTransform.offsetY),
		static_cast<int>(target->w * m_viewportTransform.scale),
		static_cast<int>(target->h * m_viewportTransform.scale),
	};
	SDL_Surface* src = SDL_CreateSurfaceFrom(
		VITA_GXM_SCREEN_WIDTH,
		VITA_GXM_SCREEN_HEIGHT,
		SDL_PIXELFORMAT_ABGR8888,
		gxm->displayBuffers[gxm->frontBufferIndex],
		VITA_GXM_SCREEN_STRIDE * 4
	);
	SDL_BlitSurfaceScaled(src, &srcRect, target, nullptr, SDL_SCALEMODE_NEAREST);
	SDL_DestroySurface(src);
}
