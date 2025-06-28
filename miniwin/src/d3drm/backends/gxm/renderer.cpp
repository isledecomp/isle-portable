#include "d3drmrenderer_gxm.h"
#include "meshutils.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <string>

#include <psp2/gxm.h>
#include <psp2/display.h>
#include <psp2/types.h>
#include <psp2/kernel/modulemgr.h>

#include "utils.h"
#include "memory.h"
#define INCBIN_PREFIX _inc_
#include "incbin.h"

bool with_razor = false;

#define VITA_GXM_SCREEN_WIDTH  960
#define VITA_GXM_SCREEN_HEIGHT 544
#define VITA_GXM_SCREEN_STRIDE 960
#define VITA_GXM_PENDING_SWAPS 2

#define VITA_GXM_COLOR_FORMAT SCE_GXM_COLOR_FORMAT_A8B8G8R8
#define VITA_GXM_PIXEL_FORMAT SCE_DISPLAY_PIXELFORMAT_A8B8G8R8



INCBIN(main_vert_gxp, "shaders/main.vert.gxp");
INCBIN(main_frag_gxp, "shaders/main.frag.gxp");
INCBIN(color_frag_gxp, "shaders/color.frag.gxp");
INCBIN(image_frag_gxp, "shaders/image.frag.gxp");

const SceGxmProgram* mainVertexProgramGxp = (const SceGxmProgram*)_inc_main_vert_gxpData;
const SceGxmProgram* mainFragmentProgramGxp = (const SceGxmProgram*)_inc_main_frag_gxpData;
const SceGxmProgram* colorFragmentProgramGxp = (const SceGxmProgram*)_inc_color_frag_gxpData;
const SceGxmProgram* imageFragmentProgramGxp = (const SceGxmProgram*)_inc_image_frag_gxpData;

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

extern "C" int sceRazorGpuCaptureSetTrigger(int frames, const char* path);
extern "C" int sceRazorGpuCaptureEnableSalvage(const char* path);
extern "C" int sceRazorGpuCaptureSetTriggerNextFrame(const char* path);

static GXMRendererContext gxm_renderer_context;

static void display_callback(const void *callback_data) {
    const GXMDisplayData *display_data = (const GXMDisplayData *)callback_data;

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

static void load_razor() {
	int mod_id = _sceKernelLoadModule("app0:librazorcapture_es4.suprx", 0, nullptr);
	int status;
	if(!SCE_ERR(sceKernelStartModule, mod_id, 0, nullptr, 0, nullptr, &status)) {
		with_razor = true;
	}

	if(with_razor) {
		sceRazorGpuCaptureEnableSalvage("ux0:data/gpu_crash.sgx");
	}
}

bool gxm_initialized = false;
bool gxm_init() {
	if(gxm_initialized) {
		return true;
	}

	load_razor();

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

	return cdramPool_init();
}

static bool create_gxm_context() {
	GXMRendererContext* data = &gxm_renderer_context;
	if(data->context) {
		return true;
	}

	if(!gxm_init()) {
		return false;
	}

	const unsigned int patcherBufferSize = 64 * 1024;
    const unsigned int patcherVertexUsseSize = 64 * 1024;
    const unsigned int patcherFragmentUsseSize = 64 * 1024;

	data->cdramPool = cdramPool_get();
	if(!data->cdramPool) {
		SDL_Log("failed to allocate cdramPool");
		return false;
	}

	// allocate buffers
	data->vdmRingBuffer = vita_mem_alloc(
        SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
        SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE,
        4,
        SCE_GXM_MEMORY_ATTRIB_READ,
        &data->vdmRingBufferUid
	);

    data->vertexRingBuffer = vita_mem_alloc(
        SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
        SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE,
        4,
        SCE_GXM_MEMORY_ATTRIB_READ,
        &data->vertexRingBufferUid
	);

    data->fragmentRingBuffer = vita_mem_alloc(
        SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
        SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE,
        4,
        SCE_GXM_MEMORY_ATTRIB_READ,
        &data->fragmentRingBufferUid
	);

    data->fragmentUsseRingBuffer = vita_mem_fragment_usse_alloc(
        SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE,
        &data->fragmentUsseRingBufferUid,
        &data->fragmentUsseRingBufferOffset);

	// create context
	SceGxmContextParams contextParams;
	memset(&contextParams, 0, sizeof(SceGxmContextParams));
    contextParams.hostMem = SDL_malloc(SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE);
    contextParams.hostMemSize = SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE;
    contextParams.vdmRingBufferMem = data->vdmRingBuffer;
    contextParams.vdmRingBufferMemSize = SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE;
    contextParams.vertexRingBufferMem = data->vertexRingBuffer;
    contextParams.vertexRingBufferMemSize = SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE;
    contextParams.fragmentRingBufferMem = data->fragmentRingBuffer;
    contextParams.fragmentRingBufferMemSize = SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE;
    contextParams.fragmentUsseRingBufferMem = data->fragmentUsseRingBuffer;
    contextParams.fragmentUsseRingBufferMemSize = SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE;
    contextParams.fragmentUsseRingBufferOffset = data->fragmentUsseRingBufferOffset;

	if(SCE_ERR(sceGxmCreateContext, &contextParams, &data->context)) {
		return false;
	}
	data->contextHostMem = contextParams.hostMem;

	// shader patcher
	data->patcherBuffer = vita_mem_alloc(
        SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
        patcherBufferSize,
        4,
        SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
        &data->patcherBufferUid
	);

    data->patcherVertexUsse = vita_mem_vertex_usse_alloc(
        patcherVertexUsseSize,
        &data->patcherVertexUsseUid,
        &data->patcherVertexUsseOffset);

    data->patcherFragmentUsse = vita_mem_fragment_usse_alloc(
        patcherFragmentUsseSize,
        &data->patcherFragmentUsseUid,
        &data->patcherFragmentUsseOffset);

	SceGxmShaderPatcherParams patcherParams;
	memset(&patcherParams, 0, sizeof(SceGxmShaderPatcherParams));
    patcherParams.userData = NULL;
    patcherParams.hostAllocCallback = &patcher_host_alloc;
    patcherParams.hostFreeCallback = &patcher_host_free;
    patcherParams.bufferAllocCallback = NULL;
    patcherParams.bufferFreeCallback = NULL;
    patcherParams.bufferMem = data->patcherBuffer;
    patcherParams.bufferMemSize = patcherBufferSize;
    patcherParams.vertexUsseAllocCallback = NULL;
    patcherParams.vertexUsseFreeCallback = NULL;
    patcherParams.vertexUsseMem = data->patcherVertexUsse;
    patcherParams.vertexUsseMemSize = patcherVertexUsseSize;
    patcherParams.vertexUsseOffset = data->patcherVertexUsseOffset;
    patcherParams.fragmentUsseAllocCallback = NULL;
    patcherParams.fragmentUsseFreeCallback = NULL;
    patcherParams.fragmentUsseMem = data->patcherFragmentUsse;
    patcherParams.fragmentUsseMemSize = patcherFragmentUsseSize;
    patcherParams.fragmentUsseOffset = data->patcherFragmentUsseOffset;

	if(SCE_ERR(sceGxmShaderPatcherCreate, &patcherParams, &data->shaderPatcher)) {
		return false;
	}
	return true;
}

static void destroy_gxm_context() {
	sceGxmShaderPatcherDestroy(gxm_renderer_context.shaderPatcher);
	sceGxmDestroyContext(gxm_renderer_context.context);
	vita_mem_fragment_usse_free(gxm_renderer_context.fragmentUsseRingBufferUid);
	vita_mem_free(gxm_renderer_context.fragmentRingBufferUid);
    vita_mem_free(gxm_renderer_context.vertexRingBufferUid);
    vita_mem_free(gxm_renderer_context.vdmRingBufferUid);
	SDL_free(gxm_renderer_context.contextHostMem);
}

bool get_gxm_context(SceGxmContext** context, SceGxmShaderPatcher** shaderPatcher, SceClibMspace* cdramPool) {
	if(!create_gxm_context()) {
		return false;
	}
	*context = gxm_renderer_context.context;
	*shaderPatcher = gxm_renderer_context.shaderPatcher;
	*cdramPool = gxm_renderer_context.cdramPool;
	return true;
}

static void CreateOrthoMatrix(float left, float right, float bottom, float top, D3DRMMATRIX4D& outMatrix)
{
	float near = -1.0f;
	float far = 1.0f;
	float rl = right - left;
	float tb = top - bottom;
	float fn = far - near;

	outMatrix[0][0] = 2.0f / rl;
	outMatrix[0][1] = 0.0f;
	outMatrix[0][2] = 0.0f;
	outMatrix[0][3] = 0.0f;

	outMatrix[1][0] = 0.0f;
	outMatrix[1][1] = 2.0f / tb;
	outMatrix[1][2] = 0.0f;
	outMatrix[1][3] = 0.0f;

	outMatrix[2][0] = 0.0f;
	outMatrix[2][1] = 0.0f;
	outMatrix[2][2] = -2.0f / fn;
	outMatrix[2][3] = 0.0f;

	outMatrix[3][0] = -(right + left) / rl;
	outMatrix[3][1] = -(top + bottom) / tb;
	outMatrix[3][2] = -(far + near) / fn;
	outMatrix[3][3] = 1.0f;
}

Direct3DRMRenderer* GXMRenderer::Create(DWORD width, DWORD height)
{
	SDL_Log("GXMRenderer::Create width=%d height=%d", width, height);

	bool success = gxm_init();
	if(!success) {
		return nullptr;
	}

	return new GXMRenderer(width, height);
}

GXMRenderer::GXMRenderer(DWORD width, DWORD height) {
	m_width = VITA_GXM_SCREEN_WIDTH;
	m_height = VITA_GXM_SCREEN_HEIGHT;
	m_virtualWidth = width;
	m_virtualHeight = height;

	const unsigned int alignedWidth = ALIGN(m_width, SCE_GXM_TILE_SIZEX);
    const unsigned int alignedHeight = ALIGN(m_height, SCE_GXM_TILE_SIZEY);
	const unsigned int sampleCount = alignedWidth * alignedHeight;
    const unsigned int depthStrideInSamples = alignedWidth;

	if(!get_gxm_context(&this->context, &this->shaderPatcher, &this->cdramPool)) return;

	// render target
	SceGxmRenderTargetParams renderTargetParams;
    memset(&renderTargetParams, 0, sizeof(SceGxmRenderTargetParams));
    renderTargetParams.flags = 0;
    renderTargetParams.width = m_width;
    renderTargetParams.height = m_height;
    renderTargetParams.scenesPerFrame = 1;
    renderTargetParams.multisampleMode = 0;
    renderTargetParams.multisampleLocations = 0;
    renderTargetParams.driverMemBlock = -1; // Invalid UID
	if(SCE_ERR(sceGxmCreateRenderTarget, &renderTargetParams, &this->renderTarget)) return;

	for(int i = 0; i < VITA_GXM_DISPLAY_BUFFER_COUNT; i++) {
		this->displayBuffers[i] = vita_mem_alloc(
			SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
			4 * VITA_GXM_SCREEN_STRIDE * VITA_GXM_SCREEN_HEIGHT,
			SCE_GXM_COLOR_SURFACE_ALIGNMENT,
			SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
			&this->displayBuffersUid[i]
		);

		if(SCE_ERR(sceGxmColorSurfaceInit,
			&this->displayBuffersSurface[i],
			SCE_GXM_COLOR_FORMAT_A8B8G8R8,
			SCE_GXM_COLOR_SURFACE_LINEAR,
			SCE_GXM_COLOR_SURFACE_SCALE_NONE,
			SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
			m_width, m_height,
			m_width,
			this->displayBuffers[i]
		)) return;

		if(SCE_ERR(sceGxmSyncObjectCreate, &this->displayBuffersSync[i])) return;
	}


	// depth & stencil
    this->depthBufferData = vita_mem_alloc(
        SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
        4 * sampleCount,
        SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
        SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
        &this->depthBufferUid
	);

    this->stencilBufferData = vita_mem_alloc(
        SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
        4 * sampleCount,
        SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
        SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
        &this->stencilBufferUid
	);

    if(SCE_ERR(sceGxmDepthStencilSurfaceInit,
        &this->depthSurface,
        SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24,
        SCE_GXM_DEPTH_STENCIL_SURFACE_TILED,
        depthStrideInSamples,
        this->depthBufferData,
        this->stencilBufferData
	)) return;

	
	// register shader programs
	if(SCE_ERR(sceGxmShaderPatcherRegisterProgram, this->shaderPatcher, colorFragmentProgramGxp, &this->colorFragmentProgramId)) return;
	if(SCE_ERR(sceGxmShaderPatcherRegisterProgram, this->shaderPatcher, mainVertexProgramGxp, &this->mainVertexProgramId)) return;
	if(SCE_ERR(sceGxmShaderPatcherRegisterProgram, this->shaderPatcher, mainFragmentProgramGxp, &this->mainFragmentProgramId)) return;
	if(SCE_ERR(sceGxmShaderPatcherRegisterProgram, this->shaderPatcher, imageFragmentProgramGxp, &this->imageFragmentProgramId)) return;

	// main shader
	{
		GET_SHADER_PARAM(positionAttribute, mainVertexProgramGxp, "aPosition",);
        GET_SHADER_PARAM(normalAttribute, mainVertexProgramGxp, "aNormal",);
		GET_SHADER_PARAM(texCoordAttribute, mainVertexProgramGxp, "aTexCoord",);

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

		vertexStreams[0].stride = sizeof(Vertex);
        vertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

		if(SCE_ERR(sceGxmShaderPatcherCreateVertexProgram,
            this->shaderPatcher,
            this->mainVertexProgramId,
            vertexAttributes, 3,
            vertexStreams, 1,
            &this->mainVertexProgram
		)) return;
	}

	// main opaque
	if(SCE_ERR(sceGxmShaderPatcherCreateFragmentProgram,
        this->shaderPatcher,
        this->mainFragmentProgramId,
        SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
        SCE_GXM_MULTISAMPLE_NONE,
        &blendInfoOpaque,
        mainVertexProgramGxp,
        &this->opaqueFragmentProgram
	)) return;

	// main transparent
	if(SCE_ERR(sceGxmShaderPatcherCreateFragmentProgram,
        this->shaderPatcher,
        this->mainFragmentProgramId,
        SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
        SCE_GXM_MULTISAMPLE_NONE,
        &blendInfoTransparent,
        mainVertexProgramGxp,
        &this->transparentFragmentProgram
	)) return;

	// image
	if(SCE_ERR(sceGxmShaderPatcherCreateFragmentProgram,
        this->shaderPatcher,
        this->imageFragmentProgramId,
        SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
        SCE_GXM_MULTISAMPLE_NONE,
        &blendInfoTransparent,
        mainVertexProgramGxp,
        &this->imageFragmentProgram
	)) return;

	// color
	if(SCE_ERR(sceGxmShaderPatcherCreateFragmentProgram,
		this->shaderPatcher,
		this->colorFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		SCE_GXM_MULTISAMPLE_NONE,
		NULL,
		mainVertexProgramGxp,
		&this->colorFragmentProgram
	)) return;

	// vertex uniforms
	this->uModelViewMatrix = sceGxmProgramFindParameterByName(mainVertexProgramGxp, "uModelViewMatrix");
	this->uNormalMatrix = sceGxmProgramFindParameterByName(mainVertexProgramGxp, "uNormalMatrix");
	this->uProjectionMatrix = sceGxmProgramFindParameterByName(mainVertexProgramGxp, "uProjectionMatrix");

	// fragment uniforms
	this->uLights = sceGxmProgramFindParameterByName(mainFragmentProgramGxp, "uLights"); // SceneLight[3]
	this->uLightCount = sceGxmProgramFindParameterByName(mainFragmentProgramGxp, "uLightCount"); // int
	this->uShininess = sceGxmProgramFindParameterByName(mainFragmentProgramGxp, "uShininess"); // float
	this->uColor = sceGxmProgramFindParameterByName(mainFragmentProgramGxp, "uColor"); // vec4
	this->uUseTexture = sceGxmProgramFindParameterByName(mainFragmentProgramGxp, "uUseTexture"); // int

	// clear uniforms
	this->colorShader_uColor = sceGxmProgramFindParameterByName(colorFragmentProgramGxp, "uColor"); // vec4

	this->lights = static_cast<decltype(this->lights)>(sceClibMspaceMalloc(this->cdramPool, sizeof(*this->lights)));
	for(int i = 0; i < VITA_GXM_UNIFORM_BUFFER_COUNT; i++) {
		this->quadVertices[i] = (Vertex*)sceClibMspaceMalloc(this->cdramPool, sizeof(Vertex)*4*50);
	}
	this->quadIndices = (uint16_t*)sceClibMspaceMalloc(this->cdramPool, sizeof(uint16_t)*4);
	this->quadIndices[0] = 0;
	this->quadIndices[1] = 1;
	this->quadIndices[2] = 2;
	this->quadIndices[3] = 3;

	volatile uint32_t *const notificationMem = sceGxmGetNotificationRegion();
	for (uint32_t i = 0; i < VITA_GXM_UNIFORM_BUFFER_COUNT; i++) {
		this->fragmentNotifications[i].address = notificationMem + (i*2);
		this->fragmentNotifications[i].value = 0;
		this->vertexNotifications[i].address = notificationMem + (i*2)+1;
		this->vertexNotifications[i].value = 0;
	}

	m_initialized = true;
}

GXMRenderer::~GXMRenderer() {
	if(!m_initialized) {
		return;
	}

	// todo free stuff

	vita_mem_free(this->depthBufferUid);
	this->depthBufferData = nullptr;
	vita_mem_free(this->stencilBufferUid);
	this->stencilBufferData = nullptr;
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
			void* textureData = sceGxmTextureGetData(&cache.gxmTexture);
			sceClibMspaceFree(ctx->renderer->cdramPool, textureData);
			delete ctx;
		},
		ctx
	);
}

static void convertTextureMetadata(SDL_Surface* surface, bool* supportedFormat, SceGxmTextureFormat* textureFormat, size_t* textureSize, size_t* textureAlignment, size_t* textureStride) {
	*supportedFormat = true;
	*textureAlignment = SCE_GXM_TEXTURE_ALIGNMENT;
	switch(surface->format) {
		case SDL_PIXELFORMAT_ABGR8888: {
			*textureFormat = SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR;
			*textureSize = surface->h * surface->pitch;
			*textureStride = surface->pitch;
			break;
		}
		/*
		case SDL_PIXELFORMAT_INDEX8: {
			*textureFormat = SCE_GXM_TEXTURE_FORMAT_P8_ABGR;
			int pixelsSize = surface->h * surface->pitch;
			int alignBytes = ALIGNMENT(pixelsSize, SCE_GXM_PALETTE_ALIGNMENT);
			*textureSize = pixelsSize + alignBytes + 0xff;
			*textureAlignment = SCE_GXM_PALETTE_ALIGNMENT;
			*textureStride = surface->pitch;
			break;
		}
		*/
		default: {
			*supportedFormat = false;
		}
	}
}

void copySurfaceTo(SDL_Surface* src, void* dstData, size_t textureStride) {
	SDL_Surface* dst = SDL_CreateSurfaceFrom(src->w, src->h, SDL_PIXELFORMAT_ABGR8888, dstData, textureStride);
	SDL_BlitSurface(src, nullptr, dst, nullptr);
	SDL_DestroySurface(dst);
}

Uint32 GXMRenderer::GetTextureId(IDirect3DRMTexture* iTexture)
{
	auto texture = static_cast<Direct3DRMTextureImpl*>(iTexture);
	auto surface = static_cast<DirectDrawSurfaceImpl*>(texture->m_surface);


	bool supportedFormat;
	size_t textureSize;
	size_t textureAlignment;
	size_t textureStride;
	SceGxmTextureFormat textureFormat;
	int textureWidth = surface->m_surface->w;
	int textureHeight = surface->m_surface->h;

	convertTextureMetadata(surface->m_surface, &supportedFormat, &textureFormat, &textureSize, &textureAlignment, &textureStride);

	if(!supportedFormat) {
		textureAlignment = SCE_GXM_TEXTURE_ALIGNMENT;
		textureStride = textureWidth * 4;
		textureSize = textureHeight * textureStride;
		textureFormat = SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR;
	}

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (tex.texture == texture) {
			if (tex.version != texture->m_version) {
				void* textureData = sceGxmTextureGetData(&tex.gxmTexture);
				if(!supportedFormat) {
					copySurfaceTo(surface->m_surface, textureData, textureStride);
				} else {
					memcpy(textureData, surface->m_surface->pixels, textureSize);
				}
				tex.version = texture->m_version;
			}
			return i;
		}
	}

	SDL_Log("Create Texture %s w=%d h=%d s=%d",
		SDL_GetPixelFormatName(surface->m_surface->format), textureWidth, textureHeight, textureStride);

	// allocate gpu memory
	void* textureData = sceClibMspaceMemalign(this->cdramPool, textureAlignment, textureSize);
	uint8_t* paletteData = nullptr;

	if(!supportedFormat) {
		SDL_Log("unsupported SDL texture format %s, falling back on SDL_PIXELFORMAT_ABGR8888", SDL_GetPixelFormatName(surface->m_surface->format));
		copySurfaceTo(surface->m_surface, textureData, textureStride);
	}
	else if(surface->m_surface->format == SDL_PIXELFORMAT_INDEX8)
	{
		LPDIRECTDRAWPALETTE _palette;
		surface->GetPalette(&_palette);
		auto palette = static_cast<DirectDrawPaletteImpl*>(_palette);

		int pixelsSize = surface->m_surface->w * surface->m_surface->h;
		int alignBytes = ALIGNMENT(pixelsSize, SCE_GXM_PALETTE_ALIGNMENT);
		SDL_Log("copying indexed texture data from=%p to=%p", surface->m_surface->pixels, textureData);
		memcpy(textureData, surface->m_surface->pixels, pixelsSize);

		paletteData = (uint8_t*)textureData + pixelsSize + alignBytes;
		memcpy(paletteData, palette->m_palette->colors, palette->m_palette->ncolors*sizeof(SDL_Color));
	}
	else
	{
		SDL_Log("copying texture data from=%p to=%p", surface->m_surface->pixels, textureData);
		memcpy(textureData, surface->m_surface->pixels, textureSize);
	}

	SceGxmTexture gxmTexture;
	SCE_ERR(sceGxmTextureInitLinearStrided, &gxmTexture, textureData, textureFormat, textureWidth, textureHeight, textureStride);
	//sceGxmTextureSetMinFilter(&gxmTexture, SCE_GXM_TEXTURE_FILTER_LINEAR);
	sceGxmTextureSetMagFilter(&gxmTexture, SCE_GXM_TEXTURE_FILTER_LINEAR);
	if(paletteData) {
		sceGxmTextureSetPalette(&gxmTexture, paletteData);
	}

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (!tex.texture) {
			tex.texture = texture;
			tex.version = texture->m_version;
			tex.gxmTexture = gxmTexture;
			tex.textureSize = textureSize;
			AddTextureDestroyCallback(i, texture);
			return i;
		}
	}

	m_textures.push_back({texture, texture->m_version, gxmTexture});
	Uint32 textureId = (Uint32) (m_textures.size() - 1);
	AddTextureDestroyCallback(textureId, texture);
	return textureId;
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

	size_t vertexBufferSize = sizeof(Vertex)*vertices.size();
	size_t indexBufferSize = sizeof(uint16_t)*indices.size();
	void* meshData = sceClibMspaceMemalign(this->cdramPool, 4, vertexBufferSize+indexBufferSize);

	Vertex* vertexBuffer = (Vertex*)meshData;
	uint16_t* indexBuffer = (uint16_t*)((uint8_t*)meshData + vertexBufferSize);

	for(int i = 0; i < vertices.size(); i++) {
		D3DRMVERTEX vertex = vertices.data()[i];
		vertexBuffer[i] = Vertex{
			.position = {
				vertex.position.x,
				vertex.position.y,
				vertex.position.z,
			},
			.normal = {
				vertex.normal.x,
				vertex.normal.y,
				vertex.normal.z,
			},
			.texCoord = {
				vertex.tu,
				vertex.tv,
			}
		};
	}
	memcpy(indexBuffer, indices.data(), indices.size()*sizeof(uint16_t));

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
			sceClibMspaceFree(ctx->renderer->cdramPool, cache.meshData);
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

void GXMRenderer::GetDesc(D3DDEVICEDESC* halDesc, D3DDEVICEDESC* helDesc)
{
	halDesc->dcmColorModel = D3DCOLORMODEL::RGB;
	halDesc->dwFlags = D3DDD_DEVICEZBUFFERBITDEPTH;
	halDesc->dwDeviceZBufferBitDepth = DDBD_16;
	halDesc->dwDeviceZBufferBitDepth |= DDBD_32;
	helDesc->dwDeviceRenderBitDepth = DDBD_32;
	halDesc->dpcTriCaps.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE;
	halDesc->dpcTriCaps.dwShadeCaps = D3DPSHADECAPS_ALPHAFLATBLEND;
	halDesc->dpcTriCaps.dwTextureFilterCaps = D3DPTFILTERCAPS_LINEAR;

	memset(helDesc, 0, sizeof(D3DDEVICEDESC));
}

const char* GXMRenderer::GetName() {
	return "GXM";
}


bool razor_triggered = false;

void GXMRenderer::StartScene() {
	if(sceneStarted) return;
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
	this->quadsUsed = 0;

	// wait for this uniform buffer to become available
	this->activeUniformBuffer = (this->activeUniformBuffer + 1) % VITA_GXM_UNIFORM_BUFFER_COUNT;
	sceGxmNotificationWait(&this->fragmentNotifications[this->activeUniformBuffer]);

	//sceClibPrintf("this->activeUniformBuffer: %d notification: %d\n", this->activeUniformBuffer, this->fragmentNotifications[this->activeUniformBuffer].value);
}

int frames = 0;
HRESULT GXMRenderer::BeginFrame()
{
	frames++;
	if(with_razor) {
		if(!razor_triggered && frames == 10) {
			SDL_Log("trigger razor");
			sceRazorGpuCaptureSetTriggerNextFrame("ux0:/data/capture.sgx");
			razor_triggered = true;
		}
	}
	this->transparencyEnabled = false;
	this->StartScene();

	auto lightData = this->LightsBuffer();
	int lightCount = std::min(static_cast<int>(m_lights.size()), 3);
	for (int i = 0; i < lightCount; ++i) {
		const auto& src = m_lights[i];
		lightData->lights[i].color[0] = src.color.r;
		lightData->lights[i].color[1] = src.color.g;
		lightData->lights[i].color[2] = src.color.b;
		lightData->lights[i].color[3] = src.color.a;

		lightData->lights[i].position[0] = src.position.x;
		lightData->lights[i].position[1] = src.position.y;
		lightData->lights[i].position[2] = src.position.z;
		lightData->lights[i].position[3] = src.positional;

		lightData->lights[i].direction[0] = src.direction.x;
		lightData->lights[i].direction[1] = src.direction.y;
		lightData->lights[i].direction[2] = src.direction.z;
		lightData->lights[i].direction[3] = src.directional;
	}
	lightData->lightCount = lightCount;
	sceGxmSetFragmentUniformBuffer(this->context, 0, lightData);

	return DD_OK;
}

void GXMRenderer::EnableTransparency() {
	this->transparencyEnabled = true;
}

static void transpose4x4(const float src[4][4], float dst[4][4]) {
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            dst[j][i] = src[i][j];
}

static const D3DRMMATRIX4D identity4x4 = {
	{1.0, 0.0, 0.0, 0.0},
	{0.0, 1.0, 0.0, 0.0},
	{0.0, 0.0, 1.0, 0.0},
	{0.0, 0.0, 0.0, 1.0},
};

static const Matrix3x3 identity3x3 = {
	{1.0, 0.0, 0.0},
	{0.0, 1.0, 0.0},
	{0.0, 0.0, 1.0},
};


void GXMRenderer::SubmitDraw(
	DWORD meshId,
	const D3DRMMATRIX4D& modelViewMatrix,
	const D3DRMMATRIX4D& worldMatrix,
	const D3DRMMATRIX4D& viewMatrix,
	const Matrix3x3& normalMatrix,
	const Appearance& appearance
) {
	auto& mesh = m_meshes[meshId];

	char marker[256];
	snprintf(marker, sizeof(marker), "SubmitDraw: %d", meshId);
	sceGxmPushUserMarker(this->context, marker);

	sceGxmSetVertexProgram(this->context, this->mainVertexProgram);
	if(this->transparencyEnabled) {
		sceGxmSetFragmentProgram(this->context, this->transparentFragmentProgram);
	} else {
    	sceGxmSetFragmentProgram(this->context, this->opaqueFragmentProgram);
	}

	void* vertUniforms;
	void* fragUniforms;
	sceGxmReserveVertexDefaultUniformBuffer(this->context, &vertUniforms);
	sceGxmReserveFragmentDefaultUniformBuffer(this->context, &fragUniforms);

	SET_UNIFORM(vertUniforms, this->uModelViewMatrix, modelViewMatrix);
	SET_UNIFORM(vertUniforms, this->uProjectionMatrix, m_projection);
	sceGxmSetUniformDataF(vertUniforms, this->uNormalMatrix, 0, 9, static_cast<const float*>(normalMatrix[0]));

	float color[4] = {
		appearance.color.r / 255.0f,
		appearance.color.g / 255.0f,
		appearance.color.b / 255.0f,
		appearance.color.a / 255.0f
	};
	SET_UNIFORM(fragUniforms, this->uColor, color);
	SET_UNIFORM(fragUniforms, this->uShininess, appearance.shininess);

	int useTexture = appearance.textureId != NO_TEXTURE_ID ? 1 : 0;
	SET_UNIFORM(fragUniforms, this->uUseTexture, useTexture);
	if(useTexture) {
		auto& texture = m_textures[appearance.textureId];
		sceGxmSetFragmentTexture(this->context, 0, &texture.gxmTexture);
	}

	sceGxmSetVertexStream(this->context, 0, mesh.vertexBuffer);
	sceGxmDraw(
		this->context,
		SCE_GXM_PRIMITIVE_TRIANGLES,
		SCE_GXM_INDEX_FORMAT_U16,
		mesh.indexBuffer,
		mesh.indexCount
	);

	sceGxmPopUserMarker(this->context);
}

HRESULT GXMRenderer::FinalizeFrame() {
	return DD_OK;
}

void GXMRenderer::Resize(int width, int height, const ViewportTransform& viewportTransform) {
	m_width = width;
	m_height = height;
	m_viewportTransform = viewportTransform;
	SDL_Log("GXMRenderer::Resize TODO");
}

void GXMRenderer::Clear(float r, float g, float b) {
	this->StartScene();

	char marker[256];
	snprintf(marker, sizeof(marker), "Clear");
	sceGxmPushUserMarker(this->context, marker);

	sceGxmSetVertexProgram(this->context, this->mainVertexProgram);
    sceGxmSetFragmentProgram(this->context, this->colorFragmentProgram);

	void* vertUniforms;
	void* fragUniforms;
	sceGxmReserveVertexDefaultUniformBuffer(this->context, &vertUniforms);
	sceGxmReserveFragmentDefaultUniformBuffer(this->context, &fragUniforms);

	D3DRMMATRIX4D projection;
	CreateOrthoMatrix(0.0, 1.0, 1.0, 0.0, projection);

	Matrix3x3 normal = {{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}};

	SET_UNIFORM(vertUniforms, this->uModelViewMatrix, identity4x4); // float4x4
	SET_UNIFORM(vertUniforms, this->uNormalMatrix, normal); // float3x3
	SET_UNIFORM(vertUniforms, this->uProjectionMatrix, projection); // float4x4

	float color[] = {r,g,b,1};
	SET_UNIFORM(fragUniforms, this->colorShader_uColor, color);

	float x1 = 0;
	float y1 = 0;
	float x2 = x1 + 1.0;
	float y2 = y1 + 1.0;

	Vertex* quadVertices = this->QuadVerticesBuffer();
	quadVertices[0] = Vertex{ .position = {x1, y1, -1.0}, .normal = {0,0,0}, .texCoord = {0,0}};
	quadVertices[1] = Vertex{ .position = {x2, y1, -1.0}, .normal = {0,0,0}, .texCoord = {0,0}};
	quadVertices[2] = Vertex{ .position = {x1, y2, -1.0}, .normal = {0,0,0}, .texCoord = {0,0}};
	quadVertices[3] = Vertex{ .position = {x2, y2, -1.0}, .normal = {0,0,0}, .texCoord = {0,0}};

	sceGxmSetVertexStream(this->context, 0, quadVertices);
	sceGxmDraw(
		this->context,
		SCE_GXM_PRIMITIVE_TRIANGLE_STRIP,
		SCE_GXM_INDEX_FORMAT_U16,
		this->quadIndices, 4
	);

	sceGxmPopUserMarker(this->context);
}

void GXMRenderer::Flip() {
	if(!this->sceneStarted) {
		this->Clear(0,0,0);
	}

	// end scene
	++this->fragmentNotifications[this->activeUniformBuffer].value;
	++this->vertexNotifications[this->activeUniformBuffer].value;
	sceGxmEndScene(
		this->context,
		nullptr, //&this->vertexNotifications[this->activeUniformBuffer],
		//nullptr
		&this->fragmentNotifications[this->activeUniformBuffer] // wait for fragment processing to finish for this buffer, otherwise lighting corrupts
	);
	sceGxmPadHeartbeat(
		&this->displayBuffersSurface[this->backBufferIndex],
		this->displayBuffersSync[this->backBufferIndex]
	);
	this->sceneStarted = false;

	// display
	GXMDisplayData displayData;
	displayData.address = this->displayBuffers[this->backBufferIndex];

	sceGxmDisplayQueueAddEntry(
		this->displayBuffersSync[this->frontBufferIndex],
		this->displayBuffersSync[this->backBufferIndex],
		&displayData
	);

	this->frontBufferIndex = this->backBufferIndex;
    this->backBufferIndex = (this->backBufferIndex + 1) % VITA_GXM_DISPLAY_BUFFER_COUNT;
}

void GXMRenderer::Draw2DImage(Uint32 textureId, const SDL_Rect& srcRect, const SDL_Rect& dstRect) {
	this->StartScene();

	char marker[256];
	snprintf(marker, sizeof(marker), "Draw2DImage: %d", textureId);
	sceGxmPushUserMarker(this->context, marker);

	sceGxmSetVertexProgram(this->context, this->mainVertexProgram);
	sceGxmSetFragmentProgram(this->context, this->imageFragmentProgram);
	
	void* vertUniforms;
	void* fragUniforms;
	sceGxmReserveVertexDefaultUniformBuffer(this->context, &vertUniforms);
	sceGxmReserveFragmentDefaultUniformBuffer(this->context, &fragUniforms);

	float left = -this->m_viewportTransform.offsetX / this->m_viewportTransform.scale;
	float right = (this->m_width - this->m_viewportTransform.offsetX) / this->m_viewportTransform.scale;
	float top = -this->m_viewportTransform.offsetY / this->m_viewportTransform.scale;
	float bottom = (this->m_height - this->m_viewportTransform.offsetY) / this->m_viewportTransform.scale;

	#define virtualToNDCX(x) (((x - left) / (right - left)));
	#define virtualToNDCY(y) (((y - top) / (bottom - top)));

	float x1_virtual = static_cast<float>(dstRect.x);
	float y1_virtual = static_cast<float>(dstRect.y);
	float x2_virtual = x1_virtual + dstRect.w;
	float y2_virtual = y1_virtual + dstRect.h;

	float x1 = virtualToNDCX(x1_virtual);
	float y1 = virtualToNDCY(y1_virtual);
	float x2 = virtualToNDCX(x2_virtual);
	float y2 = virtualToNDCY(y2_virtual);

	D3DRMMATRIX4D projection;
	CreateOrthoMatrix(0.0, 1.0, 1.0, 0.0, projection);
	static const Matrix3x3 normal = {{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}};

	D3DRMMATRIX4D identity;
	memset(identity, 0, sizeof(identity));
	identity[0][0] = 1.0f;
    identity[1][1] = 1.0f;
    identity[2][2] = 1.0f;
    identity[3][3] = 1.0f;

	SET_UNIFORM(vertUniforms, this->uModelViewMatrix, identity); // float4x4
	SET_UNIFORM(vertUniforms, this->uNormalMatrix, normal); // float3x3
	SET_UNIFORM(vertUniforms, this->uProjectionMatrix, projection); // float4x4

	const GXMTextureCacheEntry& texture = m_textures[textureId];
	sceGxmSetFragmentTexture(this->context, 0, &texture.gxmTexture);

	float texW = sceGxmTextureGetWidth(&texture.gxmTexture);
	float texH = sceGxmTextureGetHeight(&texture.gxmTexture);

	float u1 = static_cast<float>(srcRect.x) / texW;
	float v1 = static_cast<float>(srcRect.y) / texH;
	float u2 = static_cast<float>(srcRect.x + srcRect.w) / texW;
	float v2 = static_cast<float>(srcRect.y + srcRect.h) / texH;

	Vertex* quadVertices = this->QuadVerticesBuffer();
	quadVertices[0] = Vertex{ .position = {x1, y1, 0}, .normal = {0,0,0}, .texCoord = {u1, v1}};
	quadVertices[1] = Vertex{ .position = {x2, y1, 0}, .normal = {0,0,0}, .texCoord = {u2, v1}};
	quadVertices[2] = Vertex{ .position = {x1, y2, 0}, .normal = {0,0,0}, .texCoord = {u1, v2}};
	quadVertices[3] = Vertex{ .position = {x2, y2, 0}, .normal = {0,0,0}, .texCoord = {u2, v2}};

	sceGxmSetVertexStream(this->context, 0, quadVertices);

	sceGxmSetFrontDepthWriteEnable(this->context, SCE_GXM_DEPTH_WRITE_DISABLED);
	sceGxmDraw(
		this->context,
		SCE_GXM_PRIMITIVE_TRIANGLE_STRIP,
		SCE_GXM_INDEX_FORMAT_U16,
		this->quadIndices, 4
	);
	sceGxmSetFrontDepthWriteEnable(this->context, SCE_GXM_DEPTH_WRITE_ENABLED);
	sceGxmPopUserMarker(this->context);
}

void GXMRenderer::Download(SDL_Surface* target) {
	SDL_Rect srcRect = {
		static_cast<int>(m_viewportTransform.offsetX),
		static_cast<int>(m_viewportTransform.offsetY),
		static_cast<int>(target->w * m_viewportTransform.scale),
		static_cast<int>(target->h * m_viewportTransform.scale),
	};
	SDL_Surface* src = SDL_CreateSurfaceFrom(
		this->m_width, this->m_height,
		SDL_PIXELFORMAT_RGBA32,
		this->displayBuffers[this->frontBufferIndex], VITA_GXM_SCREEN_STRIDE
	);
	SDL_BlitSurfaceScaled(src, &srcRect, target, nullptr, SDL_SCALEMODE_NEAREST);
	SDL_DestroySurface(src);
}
