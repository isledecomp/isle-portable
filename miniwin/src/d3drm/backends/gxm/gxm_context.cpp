#include "gxm_context.h"

#include "gxm_memory.h"
#include "shaders/gxm_shaders.h"
#include "tlsf.h"
#include "utils.h"

#include <SDL3/SDL.h>
#include <psp2/common_dialog.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/kernel/sysmem.h>

static bool gxm_initialized = false;

bool with_razor_capture;
bool with_razor_hud;

#define CDRAM_POOL_SIZE 64 * 1024 * 1024
#define VITA_GXM_COLOR_FORMAT SCE_GXM_COLOR_FORMAT_A8B8G8R8

typedef struct GXMDisplayData {
	void* address;
	int index;
} GXMDisplayData;

static void display_callback(const void* callback_data)
{
	const GXMDisplayData* display_data = (const GXMDisplayData*) callback_data;

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
	with_razor_capture = false;
	with_razor_hud = false;
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

void GXMContext::init_cdram_allocator()
{
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
}

int GXMContext::init_context()
{
	int ret;

	const unsigned int patcherBufferSize = 64 * 1024;
	const unsigned int patcherVertexUsseSize = 64 * 1024;
	const unsigned int patcherFragmentUsseSize = 64 * 1024;

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
	return 0;
}

int GXMContext::create_display_buffers(SceGxmMultisampleMode msaaMode)
{
	int ret;

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
	this->renderTargetInit = true;

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

	return 0;
}

void GXMContext::destroy_display_buffers()
{
	if (this->renderTargetInit) {
		sceGxmFinish(this->context);
		sceGxmDestroyRenderTarget(this->renderTarget);
		this->renderTargetInit = false;
	}
	for (int i = 0; i < GXM_DISPLAY_BUFFER_COUNT; i++) {
		if (this->displayBuffers[i]) {
			vita_mem_free(this->displayBuffersUid[i]);
			this->displayBuffers[i] = nullptr;
			this->displayBuffersUid[i] = -1;
			sceGxmSyncObjectDestroy(this->displayBuffersSync[i]);
		}
	}

	if (this->depthBufferData) {
		vita_mem_free(this->depthBufferUid);
		this->depthBufferData = nullptr;
		this->depthBufferUid = -1;
	}

	if (this->stencilBufferData) {
		vita_mem_free(this->stencilBufferUid);
		this->stencilBufferData = nullptr;
		this->stencilBufferUid = -1;
	}
}

void GXMContext::init_clear_mesh()
{
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
}

int GXMContext::register_base_shaders()
{
	int ret;
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
	this->color_uColor = sceGxmProgramFindParameterByName(colorFragmentProgramGxp, "uColor"); // vec4
	return 0;
}

int GXMContext::patch_base_shaders(SceGxmMultisampleMode msaaMode)
{
	int ret;
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
			msaaMode,
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
			msaaMode,
			&blendInfoTransparent,
			planeVertexProgramGxp,
			&this->imageFragmentProgram
		);
		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}

void GXMContext::destroy_base_shaders()
{
	sceGxmShaderPatcherReleaseVertexProgram(this->shaderPatcher, this->planeVertexProgram);
	sceGxmShaderPatcherReleaseFragmentProgram(this->shaderPatcher, this->colorFragmentProgram);
	sceGxmShaderPatcherReleaseFragmentProgram(this->shaderPatcher, this->imageFragmentProgram);
}

int GXMContext::init(SceGxmMultisampleMode msaaMode)
{
	int ret = 0;
	ret = gxm_library_init();
	if (ret < 0) {
		return ret;
	}
	if (this->cdramPool == nullptr) {
		this->init_cdram_allocator();
	}

	if (this->context == nullptr) {
		ret = this->init_context();
		if (ret < 0) {
			return ret;
		}
	}

	if (this->planeVertexProgramId == 0) {
		ret = this->register_base_shaders();
		if (ret < 0) {
			return ret;
		}
	}

	if (this->clearVertices == nullptr) {
		this->init_clear_mesh();
	}

	// recreate when msaa is different
	if (msaaMode != this->displayMsaa && this->renderTargetInit) {
		this->destroy_display_buffers();
		this->destroy_base_shaders();
	}

	if (!this->renderTargetInit) {
		ret = this->create_display_buffers(msaaMode);
		if (ret < 0) {
			return ret;
		}
		ret = this->patch_base_shaders(msaaMode);
		if (ret < 0) {
			return ret;
		}
	}
	return 0;
}

static int inuse_mem = 0;
void* GXMContext::alloc(size_t size, size_t align)
{
	if (this->cdramPool == nullptr) {
		this->init_cdram_allocator();
	}
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

	this->destroy_display_buffers();
	this->destroy_base_shaders();

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
