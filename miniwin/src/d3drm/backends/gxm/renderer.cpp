#include "d3drmrenderer_gxm.h"
#include "meshutils.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <string>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

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
#define VITA_GXM_PENDING_SWAPS 3

#define VITA_GXM_COLOR_FORMAT SCE_GXM_COLOR_FORMAT_A8B8G8R8
#define VITA_GXM_PIXEL_FORMAT SCE_DISPLAY_PIXELFORMAT_A8B8G8R8

struct SceneLightGXM {
	float color[4];
	float position[4];
	float direction[4];
};

typedef struct Vertex {
	float position[3];
    float normal[3];
    float texCoord[2];
} Vertex;


INCBIN(main_vert_gxp, "shaders/main.vert.gxp");
INCBIN(main_frag_gxp, "shaders/main.frag.gxp");
INCBIN(clear_vert_gxp, "shaders/clear.vert.gxp");
INCBIN(clear_frag_gxp, "shaders/clear.frag.gxp");
INCBIN(image_frag_gxp, "shaders/image.frag.gxp");

const SceGxmProgram* mainVertexProgramGxp = (const SceGxmProgram*)_inc_main_vert_gxpData;
const SceGxmProgram* mainFragmentProgramGxp = (const SceGxmProgram*)_inc_main_frag_gxpData;
const SceGxmProgram* clearVertexProgramGxp = (const SceGxmProgram*)_inc_clear_vert_gxpData;
const SceGxmProgram* clearFragmentProgramGxp = (const SceGxmProgram*)_inc_clear_frag_gxpData;
const SceGxmProgram* imageFragmentProgramGxp = (const SceGxmProgram*)_inc_image_frag_gxpData;

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
        &data->vdmRingBufferUid,
		"vdmRingBuffer",
		nullptr
	);

    data->vertexRingBuffer = vita_mem_alloc(
        SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
        SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE,
        4,
        SCE_GXM_MEMORY_ATTRIB_READ,
        &data->vertexRingBufferUid,
		"vertexRingBuffer",
		nullptr
	);

    data->fragmentRingBuffer = vita_mem_alloc(
        SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
        SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE,
        4,
        SCE_GXM_MEMORY_ATTRIB_READ,
        &data->fragmentRingBufferUid,
		"fragmentRingBuffer",
		nullptr
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
        &data->patcherBufferUid,
		"patcherBuffer",
		nullptr
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


static bool create_gxm_renderer(int width, int height, GXMRendererData* data) {
	const unsigned int alignedWidth = ALIGN(VITA_GXM_SCREEN_WIDTH, SCE_GXM_TILE_SIZEX);
    const unsigned int alignedHeight = ALIGN(VITA_GXM_SCREEN_HEIGHT, SCE_GXM_TILE_SIZEY);

	unsigned int sampleCount = alignedWidth * alignedHeight;
    unsigned int depthStrideInSamples = alignedWidth;

	if(!get_gxm_context(&data->context, &data->shaderPatcher, &data->cdramPool)) {
		return false;
	}

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

	// render target
	SceGxmRenderTargetParams renderTargetParams;
    memset(&renderTargetParams, 0, sizeof(SceGxmRenderTargetParams));
    renderTargetParams.flags = 0;
    renderTargetParams.width = VITA_GXM_SCREEN_WIDTH;
    renderTargetParams.height = VITA_GXM_SCREEN_HEIGHT;
    renderTargetParams.scenesPerFrame = 1;
    renderTargetParams.multisampleMode = 0;
    renderTargetParams.multisampleLocations = 0;
    renderTargetParams.driverMemBlock = -1; // Invalid UID

	if(SCE_ERR(sceGxmCreateRenderTarget, &renderTargetParams, &data->renderTarget)) {
		return false;
	}

	for(int i = 0; i < VITA_GXM_DISPLAY_BUFFER_COUNT; i++) {
		data->displayBuffers[i] = vita_mem_alloc(
			SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
			4 * VITA_GXM_SCREEN_STRIDE * VITA_GXM_SCREEN_HEIGHT,
			SCE_GXM_COLOR_SURFACE_ALIGNMENT,
			SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
			&data->displayBuffersUid[i], "display", nullptr);

		if(SCE_ERR(sceGxmColorSurfaceInit,
			&data->displayBuffersSurface[i],
			SCE_GXM_COLOR_FORMAT_A8B8G8R8,
			SCE_GXM_COLOR_SURFACE_LINEAR,
			SCE_GXM_COLOR_SURFACE_SCALE_NONE,
			SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
			VITA_GXM_SCREEN_WIDTH, VITA_GXM_SCREEN_HEIGHT,
			VITA_GXM_SCREEN_STRIDE,
			data->displayBuffers[i]
		)) {
			return false;
		};

		if(SCE_ERR(sceGxmSyncObjectCreate, &data->displayBuffersSync[i])) {
			return false;
		}
	}


	// depth & stencil
    data->depthBufferData = vita_mem_alloc(
        SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
        4 * sampleCount,
        SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
        SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
        &data->depthBufferUid,
		"depthBufferData",
		nullptr
	);

    data->stencilBufferData = vita_mem_alloc(
        SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
        4 * sampleCount,
        SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
        SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
        &data->stencilBufferUid,
		"stencilBufferData",
		nullptr
	);

    if(SCE_ERR(sceGxmDepthStencilSurfaceInit,
        &data->depthSurface,
        SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24,
        SCE_GXM_DEPTH_STENCIL_SURFACE_TILED,
        depthStrideInSamples,
        data->depthBufferData,
        data->stencilBufferData
	)) {
		return false;
	}

	// register shader programs
	if(SCE_ERR(sceGxmShaderPatcherRegisterProgram, data->shaderPatcher, clearVertexProgramGxp, &data->clearVertexProgramId)) {
		return false;
	}
	if(SCE_ERR(sceGxmShaderPatcherRegisterProgram, data->shaderPatcher, clearFragmentProgramGxp, &data->clearFragmentProgramId)) {
		return false;
	}
	if(SCE_ERR(sceGxmShaderPatcherRegisterProgram, data->shaderPatcher, mainVertexProgramGxp, &data->mainVertexProgramId)) {
		return false;
	}
	if(SCE_ERR(sceGxmShaderPatcherRegisterProgram, data->shaderPatcher, mainFragmentProgramGxp, &data->mainFragmentProgramId)) {
		return false;
	}
	if(SCE_ERR(sceGxmShaderPatcherRegisterProgram, data->shaderPatcher, imageFragmentProgramGxp, &data->imageFragmentProgramId)) {
		return false;
	}

	
	// clear shader
	{
		GET_SHADER_PARAM(positionAttribute, clearVertexProgramGxp, "aPosition", false);

		SceGxmVertexAttribute vertexAttributes[1];
		SceGxmVertexStream vertexStreams[1];
		vertexAttributes[0].streamIndex = 0;
		vertexAttributes[0].offset = 0;
		vertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
		vertexAttributes[0].componentCount = 2;
		vertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(positionAttribute);
		vertexStreams[0].stride = sizeof(float)*2;
		vertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

		if(SCE_ERR(sceGxmShaderPatcherCreateVertexProgram,
			data->shaderPatcher,
			data->clearVertexProgramId,
			vertexAttributes, 1,
			vertexStreams, 1,
			&data->clearVertexProgram
		)) {
			return false;
		}

		if(SCE_ERR(sceGxmShaderPatcherCreateFragmentProgram,
			data->shaderPatcher,
			data->clearFragmentProgramId,
			SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
			SCE_GXM_MULTISAMPLE_NONE,
			NULL,
			clearVertexProgramGxp,
			&data->clearFragmentProgram
		)) {
			return false;
		}
	}

	// main shader
	{
		GET_SHADER_PARAM(positionAttribute, mainVertexProgramGxp, "aPosition", false);
        GET_SHADER_PARAM(normalAttribute, mainVertexProgramGxp, "aNormal", false);
		GET_SHADER_PARAM(texCoordAttribute, mainVertexProgramGxp, "aTexCoord", false);

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
            data->shaderPatcher,
            data->mainVertexProgramId,
            vertexAttributes, 3,
            vertexStreams, 1,
            &data->mainVertexProgram
		)) return false;
	}

	// main opaque
	if(SCE_ERR(sceGxmShaderPatcherCreateFragmentProgram,
        data->shaderPatcher,
        data->mainFragmentProgramId,
        SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
        SCE_GXM_MULTISAMPLE_NONE,
        &blendInfoOpaque,
        mainVertexProgramGxp,
        &data->opaqueFragmentProgram
	)) return false;

	// main transparent
	if(SCE_ERR(sceGxmShaderPatcherCreateFragmentProgram,
        data->shaderPatcher,
        data->mainFragmentProgramId,
        SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
        SCE_GXM_MULTISAMPLE_NONE,
        &blendInfoTransparent,
        mainVertexProgramGxp,
        &data->transparentFragmentProgram
	)) return false;

	// image
	if(SCE_ERR(sceGxmShaderPatcherCreateFragmentProgram,
        data->shaderPatcher,
        data->imageFragmentProgramId,
        SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
        SCE_GXM_MULTISAMPLE_NONE,
        &blendInfoTransparent,
        mainVertexProgramGxp,
        &data->imageFragmentProgram
	)) return false;

	// vertex uniforms
	data->uModelViewMatrix = sceGxmProgramFindParameterByName(mainVertexProgramGxp, "uModelViewMatrix");
	data->uNormalMatrix = sceGxmProgramFindParameterByName(mainVertexProgramGxp, "uNormalMatrix");
	data->uProjectionMatrix = sceGxmProgramFindParameterByName(mainVertexProgramGxp, "uProjectionMatrix");

	// fragment uniforms
	data->uLights = sceGxmProgramFindParameterByName(mainFragmentProgramGxp, "uLights"); // SceneLight[3]
	data->uLightCount = sceGxmProgramFindParameterByName(mainFragmentProgramGxp, "uLightCount"); // int
	data->uShininess = sceGxmProgramFindParameterByName(mainFragmentProgramGxp, "uShininess"); // float
	data->uColor = sceGxmProgramFindParameterByName(mainFragmentProgramGxp, "uColor"); // vec4
	data->uUseTexture = sceGxmProgramFindParameterByName(mainFragmentProgramGxp, "uUseTexture"); // int

	// clear uniforms
	data->clearShader_uColor = sceGxmProgramFindParameterByName(clearFragmentProgramGxp, "uColor"); // vec4
	
	// clear mesh
	const size_t clearMeshVerticiesSize = 3 * sizeof(float)*2;
	const size_t clearMeshIndiciesSize = 3 * sizeof(uint16_t);
	data->clearMeshBuffer = sceClibMspaceMalloc(data->cdramPool, clearMeshVerticiesSize + clearMeshIndiciesSize);
	data->clearVerticies = (float*)data->clearMeshBuffer;
	data->clearIndicies = (uint16_t*)((uint8_t*)(data->clearMeshBuffer)+clearMeshVerticiesSize);

	data->clearVerticies[0] = -1.0 * width;
	data->clearVerticies[1] = -1.0 * height;
	data->clearVerticies[2] = 3.0 * width;
	data->clearVerticies[3] = -1.0 * height;
	data->clearVerticies[4] = -1.0;
	data->clearVerticies[5] = 3.0;

	data->clearIndicies[0] = 0;
	data->clearIndicies[1] = 1;
	data->clearIndicies[2] = 2;

	// light uniforms buffer
	data->lightDataBuffer = sceClibMspaceMalloc(data->cdramPool,
		3 * sizeof(SceneLightGXM) + 4 // 3 lights + light count
	);
	return true;
}


Direct3DRMRenderer* GXMRenderer::Create(DWORD width, DWORD height)
{
	SDL_Log("GXMRenderer::Create width=%d height=%d", width, height);

	bool success = gxm_init();
	if(!success) {
		return nullptr;
	}

	GXMRendererData gxm_data;
	success = create_gxm_renderer(width, height, &gxm_data);
	if(!success) {
		return nullptr;
	}

	return new GXMRenderer(width, height, gxm_data);
}

GXMRenderer::GXMRenderer(
	DWORD width,
	DWORD height,
	GXMRendererData data
) : m_data(data) {
	m_width = width;
	m_height = height;
	m_virtualWidth = width;
	m_virtualHeight = height;
	m_initialized = true;
}

GXMRenderer::~GXMRenderer() {
	if(!m_initialized) {
		return;
	}

	// todo free stuff

	vita_mem_free(this->m_data.depthBufferUid);
	this->m_data.depthBufferData = nullptr;
	vita_mem_free(this->m_data.stencilBufferUid);
	this->m_data.stencilBufferData = nullptr;
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
	//m_projection[1][1] *= -1.0f; // OpenGL is upside down
}

struct TextureDestroyContextGLS2 {
	GXMRenderer* renderer;
	Uint32 textureId;
};

void GXMRenderer::AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture)
{
	auto* ctx = new TextureDestroyContextGLS2{this, id};
	texture->AddDestroyCallback(
		[](IDirect3DRMObject* obj, void* arg) {
			auto* ctx = static_cast<TextureDestroyContextGLS2*>(arg);
			auto& cache = ctx->renderer->m_textures[ctx->textureId];
			void* textureData = sceGxmTextureGetData(&cache.gxmTexture);
			sceClibMspaceFree(ctx->renderer->m_data.cdramPool, textureData);
			delete ctx;
		},
		ctx
	);
}

Uint32 GXMRenderer::GetTextureId(IDirect3DRMTexture* iTexture)
{
	auto texture = static_cast<Direct3DRMTextureImpl*>(iTexture);
	auto surface = static_cast<DirectDrawSurfaceImpl*>(texture->m_surface);

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (tex.texture == texture) {
			if (tex.version != texture->m_version) {
				SDL_Surface* surf = SDL_ConvertSurface(surface->m_surface, SDL_PIXELFORMAT_ABGR8888);
				if (!surf) {
					return NO_TEXTURE_ID;
				}
				void* textureData = sceGxmTextureGetData(&tex.gxmTexture);
				memcpy(textureData, surf->pixels, surf->w*surf->h*4);
				SDL_DestroySurface(surf);
				tex.version = texture->m_version;
			}
			return i;
		}
	}

	SDL_Surface* surf = SDL_ConvertSurface(surface->m_surface, SDL_PIXELFORMAT_ABGR8888);
	if (!surf) {
		return NO_TEXTURE_ID;
	}
	void* textureData = sceClibMspaceMemalign(this->m_data.cdramPool, SCE_GXM_TEXTURE_ALIGNMENT, surf->w*surf->h*4);
	memcpy(textureData, surf->pixels, surf->w*surf->h*4);
	SDL_DestroySurface(surf);

	SceGxmTexture gxmTexture;
	sceGxmTextureInitLinear(&gxmTexture, textureData, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_RGBA, surf->w, surf->h, 1);
	sceGxmTextureSetMinFilter(&gxmTexture, SCE_GXM_TEXTURE_FILTER_LINEAR);
	sceGxmTextureSetMagFilter(&gxmTexture, SCE_GXM_TEXTURE_FILTER_LINEAR);

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (!tex.texture) {
			tex.texture = texture;
			tex.version = texture->m_version;
			tex.gxmTexture = gxmTexture;
			AddTextureDestroyCallback(i, texture);
			return i;
		}
	}

	m_textures.push_back({texture, texture->m_version, gxmTexture});
	AddTextureDestroyCallback((Uint32) (m_textures.size() - 1), texture);
	return (Uint32) (m_textures.size() - 1);
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
	void* meshData = sceClibMspaceMemalign(this->m_data.cdramPool, 4, vertexBufferSize+indexBufferSize);

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
			sceClibMspaceFree(ctx->renderer->m_data.cdramPool, cache.meshData);
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
		this->m_data.context,
		0,
		this->m_data.renderTarget,
		nullptr,
		nullptr,
		this->m_data.displayBuffersSync[this->backBufferIndex],
		&this->m_data.displayBuffersSurface[this->backBufferIndex],
		&this->m_data.depthSurface
	);
	sceGxmSetViewport(this->m_data.context, 0, m_width, 0, m_height, 0, 0);
	this->sceneStarted = true;
}

HRESULT GXMRenderer::BeginFrame()
{
	if(with_razor && !razor_triggered) {
		SDL_Log("trigger razor for next frame");
		sceRazorGpuCaptureSetTriggerNextFrame("ux0:/data/capture.sgx");
		razor_triggered = true;
	}
	this->transparencyEnabled = false;

	this->StartScene();

	sceGxmSetFragmentUniformBuffer(this->m_data.context, 0, this->m_data.lightDataBuffer);

	sceGxmSetFrontStencilRef(this->m_data.context, 1);
	sceGxmSetFrontStencilFunc(
        this->m_data.context,
        SCE_GXM_STENCIL_FUNC_ALWAYS,
        SCE_GXM_STENCIL_OP_KEEP,
        SCE_GXM_STENCIL_OP_KEEP,
        SCE_GXM_STENCIL_OP_KEEP,
        0xFF,
        0xFF
	);
	sceGxmSetFrontDepthFunc(this->m_data.context, SCE_GXM_DEPTH_FUNC_ALWAYS);

	// set light data
	int lightCount = std::min(static_cast<int>(m_lights.size()), 3);

	SceneLightGXM* lightData = (SceneLightGXM*)this->m_data.lightDataBuffer;
	int* pLightCount = (int*)((uint8_t*)(this->m_data.lightDataBuffer)+sizeof(SceneLightGXM)*3);
	*pLightCount = lightCount;

	for (int i = 0; i < lightCount; ++i) {
		const auto& src = m_lights[i];
		lightData[i].color[0] = src.color.r;
		lightData[i].color[1] = src.color.g;
		lightData[i].color[2] = src.color.b;
		lightData[i].color[3] = src.color.a;

		lightData[i].position[0] = src.position.x;
		lightData[i].position[1] = src.position.y;
		lightData[i].position[2] = src.position.z;
		lightData[i].position[3] = src.positional;

		lightData[i].direction[0] = src.direction.x;
		lightData[i].direction[1] = src.direction.y;
		lightData[i].direction[2] = src.direction.z;
		lightData[i].direction[3] = src.directional;
	}
	return DD_OK;
}

void GXMRenderer::EnableTransparency() {
	this->transparencyEnabled = true;
}

void transpose4x4(const float src[4][4], float dst[4][4]) {
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            dst[j][i] = src[i][j];
}

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
	sceGxmPushUserMarker(this->m_data.context, marker);

	sceGxmSetVertexProgram(this->m_data.context, this->m_data.mainVertexProgram);
	if(this->transparencyEnabled) {
		sceGxmSetFragmentProgram(this->m_data.context, this->m_data.transparentFragmentProgram);
	} else {
    	sceGxmSetFragmentProgram(this->m_data.context, this->m_data.opaqueFragmentProgram);
	}

	void* vertUniforms;
	void* fragUniforms;
	sceGxmReserveVertexDefaultUniformBuffer(this->m_data.context, &vertUniforms);
	sceGxmReserveFragmentDefaultUniformBuffer(this->m_data.context, &fragUniforms);

	D3DRMMATRIX4D modelViewMatrixTrans;
	D3DRMMATRIX4D projectionTrans;
	transpose4x4(modelViewMatrix, modelViewMatrixTrans);
	transpose4x4(m_projection, projectionTrans);

	SET_UNIFORM(vertUniforms, this->m_data.uModelViewMatrix, modelViewMatrixTrans, mainVertexProgramGxp);
	SET_UNIFORM(vertUniforms, this->m_data.uNormalMatrix, normalMatrix, mainVertexProgramGxp);
	SET_UNIFORM(vertUniforms, this->m_data.uProjectionMatrix, projectionTrans, mainVertexProgramGxp);
	
	float color[4] = {
		appearance.color.r / 255.0f,
		appearance.color.g / 255.0f,
		appearance.color.b / 255.0f,
		appearance.color.a / 255.0f
	};
	SET_UNIFORM(fragUniforms, this->m_data.uColor, color, mainFragmentProgramGxp);
	SET_UNIFORM(fragUniforms, this->m_data.uShininess, appearance.shininess, mainFragmentProgramGxp);

	int useTexture = appearance.textureId != NO_TEXTURE_ID ? 1 : 0;
	SET_UNIFORM(fragUniforms, this->m_data.uUseTexture, useTexture, mainFragmentProgramGxp);
	if(useTexture) {
		auto& texture = m_textures[appearance.textureId];
		sceGxmSetFragmentTexture(this->m_data.context, 0, &texture.gxmTexture);
	}

	sceGxmSetVertexStream(this->m_data.context, 0, mesh.vertexBuffer);
	sceGxmDraw(
		this->m_data.context,
		SCE_GXM_PRIMITIVE_TRIANGLES,
		SCE_GXM_INDEX_FORMAT_U16,
		mesh.indexBuffer,
		mesh.indexCount
	);

	sceGxmPopUserMarker(this->m_data.context);
}

HRESULT GXMRenderer::FinalizeFrame() {
	return DD_OK;
}

void GXMRenderer::Resize(int width, int height, const ViewportTransform& viewportTransform) {
	m_width = width;
	m_height = height;
	m_viewportTransform = viewportTransform;
}

void GXMRenderer::Clear(float r, float g, float b) {
	this->StartScene();

	char marker[256];
	snprintf(marker, sizeof(marker), "Clear");
	sceGxmPushUserMarker(this->m_data.context, marker);

	sceGxmSetVertexProgram(this->m_data.context, this->m_data.clearVertexProgram);
    sceGxmSetFragmentProgram(this->m_data.context, this->m_data.clearFragmentProgram);

	void* vertUniforms;
	void* fragUniforms;
	sceGxmReserveVertexDefaultUniformBuffer(this->m_data.context, &vertUniforms);
	sceGxmReserveFragmentDefaultUniformBuffer(this->m_data.context, &fragUniforms);

	float color[] = {r,g,b,1};
	SET_UNIFORM(fragUniforms, this->m_data.clearShader_uColor, color, clearFragmentProgramGxp);
	
	sceGxmSetVertexStream(this->m_data.context, 0, this->m_data.clearVerticies);
	sceGxmDraw(
		this->m_data.context,
		SCE_GXM_PRIMITIVE_TRIANGLES,
		SCE_GXM_INDEX_FORMAT_U16,
		this->m_data.clearIndicies, 3
	);

	sceGxmPopUserMarker(this->m_data.context);
}

void GXMRenderer::Flip() {
	if(this->sceneStarted) {
		sceGxmEndScene(
			this->m_data.context,
			nullptr, nullptr
		);

		sceGxmPadHeartbeat(
			&this->m_data.displayBuffersSurface[this->backBufferIndex],
			this->m_data.displayBuffersSync[this->backBufferIndex]
		);
		this->sceneStarted = false;
	}

	GXMDisplayData displayData;
	displayData.address = this->m_data.displayBuffers[this->backBufferIndex];

	sceGxmDisplayQueueAddEntry(
		this->m_data.displayBuffersSync[this->frontBufferIndex],
		this->m_data.displayBuffersSync[this->backBufferIndex],
		&displayData
	);

	this->frontBufferIndex = this->backBufferIndex;
    this->backBufferIndex = (this->backBufferIndex + 1) % VITA_GXM_DISPLAY_BUFFER_COUNT;
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

void GXMRenderer::Draw2DImage(Uint32 textureId, const SDL_Rect& srcRect, const SDL_Rect& dstRect) {
	this->StartScene();

	char marker[256];
	snprintf(marker, sizeof(marker), "Draw2DImage: %d", textureId);
	sceGxmPushUserMarker(this->m_data.context, marker);

	sceGxmSetVertexProgram(this->m_data.context, this->m_data.mainVertexProgram);
	sceGxmSetFragmentProgram(this->m_data.context, this->m_data.imageFragmentProgram);
	
	void* vertUniforms;
	void* fragUniforms;
	sceGxmReserveVertexDefaultUniformBuffer(this->m_data.context, &vertUniforms);
	sceGxmReserveFragmentDefaultUniformBuffer(this->m_data.context, &fragUniforms);

	float left = -m_viewportTransform.offsetX / m_viewportTransform.scale;
	float right = (m_width - m_viewportTransform.offsetX) / m_viewportTransform.scale;
	float top = -m_viewportTransform.offsetY / m_viewportTransform.scale;
	float bottom = (m_height - m_viewportTransform.offsetY) / m_viewportTransform.scale;

	D3DRMMATRIX4D projection;
	CreateOrthoMatrix(left, right, bottom, top, projection);

	D3DRMMATRIX4D identity = {{1.f, 0.f, 0.f, 0.f}, {0.f, 1.f, 0.f, 0.f}, {0.f, 0.f, 1.f, 0.f}, {0.f, 0.f, 0.f, 1.f}};
	Matrix3x3 normal = {{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}};

	SET_UNIFORM(vertUniforms, this->m_data.uModelViewMatrix, identity, mainVertexProgramGxp); // float4x4
	SET_UNIFORM(vertUniforms, this->m_data.uNormalMatrix, normal, mainVertexProgramGxp); // float3x3
	SET_UNIFORM(vertUniforms, this->m_data.uProjectionMatrix, projection, mainVertexProgramGxp); // float4x4

	const GXMTextureCacheEntry& texture = m_textures[textureId];
	sceGxmSetFragmentTexture(this->m_data.context, 0, &texture.gxmTexture);

	float texW = sceGxmTextureGetWidth(&texture.gxmTexture);
	float texH = sceGxmTextureGetHeight(&texture.gxmTexture);

	float u1 = srcRect.x / texW;
	float v1 = srcRect.y / texH;
	float u2 = (srcRect.x + srcRect.w) / texW;
	float v2 = (srcRect.y + srcRect.h) / texH;

	float x1 = static_cast<float>(dstRect.x);
	float y1 = static_cast<float>(dstRect.y);
	float x2 = x1 + dstRect.w;
	float y2 = y1 + dstRect.h;

	void* meshBuffer = sceClibMspaceMalloc(this->m_data.cdramPool, 4*sizeof(Vertex) + 4*sizeof(uint16_t));
	Vertex* gpuVertices = (Vertex*)meshBuffer;
	uint16_t* gpuIndices = (uint16_t*)(((uint8_t*)meshBuffer) + 4*sizeof(Vertex));
	
	gpuVertices[0] = Vertex{ .position = {x1, y1, 0}, .normal = {0,0,0}, .texCoord = {u1, v1}};
	gpuVertices[1] = Vertex{ .position = {x2, y1, 0}, .normal = {0,0,0}, .texCoord = {u2, v1}};
	gpuVertices[2] = Vertex{ .position = {x1, y2, 0}, .normal = {0,0,0}, .texCoord = {u1, v2}};
	gpuVertices[3] = Vertex{ .position = {x2, y2, 0}, .normal = {0,0,0}, .texCoord = {u2, v2}};

	gpuIndices[0] = 0;
	gpuIndices[1] = 1;
	gpuIndices[2] = 2;
	gpuIndices[3] = 3;

	sceGxmSetVertexStream(this->m_data.context, 0, gpuVertices);
	sceGxmDraw(
		this->m_data.context,
		SCE_GXM_PRIMITIVE_TRIANGLE_STRIP,
		SCE_GXM_INDEX_FORMAT_U16,
		gpuIndices, 4
	);

	sceGxmPopUserMarker(this->m_data.context);
	sceClibMspaceFree(this->m_data.cdramPool, meshBuffer);
}

void GXMRenderer::Download(SDL_Surface* target) {
	SDL_Surface* src = SDL_CreateSurfaceFrom(
		this->m_width, this->m_height,
		SDL_PIXELFORMAT_RGBA32,
		this->m_data.displayBuffers[this->backBufferIndex], VITA_GXM_SCREEN_STRIDE
	);

	SDL_Rect srcRect = {
		static_cast<int>(m_viewportTransform.offsetX),
		static_cast<int>(m_viewportTransform.offsetY),
		static_cast<int>(target->w * m_viewportTransform.scale),
		static_cast<int>(target->h * m_viewportTransform.scale),
	};

	SDL_BlitSurfaceScaled(src, &srcRect, target, nullptr, SDL_SCALEMODE_NEAREST);
}
