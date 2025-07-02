#include "d3drmrenderer_gxm.h"
#include "gxm_memory.h"
#include "meshutils.h"
#include "razor.h"
#include "tlsf.h"
#include "utils.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/types.h>
#include <string>
#define INCBIN_PREFIX _inc_
#include "incbin.h"

#include <assert.h>

bool with_razor = false;
bool with_razor_hud = false;
bool gxm_initialized = false;

#define VITA_GXM_SCREEN_WIDTH 960
#define VITA_GXM_SCREEN_HEIGHT 544
#define VITA_GXM_SCREEN_STRIDE 960
#define VITA_GXM_PENDING_SWAPS 2

#define VITA_GXM_COLOR_FORMAT SCE_GXM_COLOR_FORMAT_A8B8G8R8
#define VITA_GXM_PIXEL_FORMAT SCE_DISPLAY_PIXELFORMAT_A8B8G8R8

#define SCE_GXM_PRECOMPUTED_ALIGNMENT 16

INCBIN(main_vert_gxp, "shaders/main.vert.gxp");
INCBIN(main_color_frag_gxp, "shaders/main.color.frag.gxp");
INCBIN(main_texture_frag_gxp, "shaders/main.texture.frag.gxp");
INCBIN(color_frag_gxp, "shaders/color.frag.gxp");
INCBIN(image_frag_gxp, "shaders/image.frag.gxp");

const SceGxmProgram* mainVertexProgramGxp = (const SceGxmProgram*) _inc_main_vert_gxpData;
const SceGxmProgram* mainColorFragmentProgramGxp = (const SceGxmProgram*) _inc_main_color_frag_gxpData;
const SceGxmProgram* mainTextureFragmentProgramGxp = (const SceGxmProgram*) _inc_main_texture_frag_gxpData;
const SceGxmProgram* colorFragmentProgramGxp = (const SceGxmProgram*) _inc_color_frag_gxpData;
const SceGxmProgram* imageFragmentProgramGxp = (const SceGxmProgram*) _inc_image_frag_gxpData;

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

static GXMRendererContext gxm_renderer_context;

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

static const bool extra_debug = false;

static void load_razor()
{
	if (load_suprx("app0:librazorcapture_es4.suprx") >= 0) {
		with_razor = true;
	}

	if (extra_debug) {
		load_skprx("ux0:app/LEGO00001/syslibtrace.skprx");
		load_skprx("ux0:app/LEGO00001/pamgr.skprx");

		if (load_suprx("app0:libperf.suprx") >= 0) {
		}

		if (load_suprx("app0:librazorhud_es4.suprx") >= 0) {
			with_razor_hud = true;
		}
	}

	if (with_razor) {
		//sceRazorGpuCaptureEnableSalvage("ux0:data/gpu_crash.sgx");
	}

	if (with_razor_hud) {
		sceRazorGpuTraceSetFilename("ux0:data/gpu_trace", 3);
	}
}

bool gxm_init()
{
	if (gxm_initialized) {
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
	return true;
}

static bool create_gxm_context()
{
	GXMRendererContext* data = &gxm_renderer_context;
	if (data->context) {
		return true;
	}

	if (!gxm_init()) {
		return false;
	}

	const unsigned int patcherBufferSize = 64 * 1024;
	const unsigned int patcherVertexUsseSize = 64 * 1024;
	const unsigned int patcherFragmentUsseSize = 64 * 1024;

	// allocate buffers
	data->vdmRingBuffer = vita_mem_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&data->vdmRingBufferUid,
		"vdmRingBuffer"
	);

	data->vertexRingBuffer = vita_mem_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&data->vertexRingBufferUid,
		"vertexRingBuffer"
	);

	data->fragmentRingBuffer = vita_mem_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&data->fragmentRingBufferUid,
		"fragmentRingBuffer"
	);

	data->fragmentUsseRingBuffer = vita_mem_fragment_usse_alloc(
		SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE,
		&data->fragmentUsseRingBufferUid,
		&data->fragmentUsseRingBufferOffset
	);

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

	if (SCE_ERR(sceGxmCreateContext, &contextParams, &data->context)) {
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
		"patcherBuffer"
	);

	data->patcherVertexUsse =
		vita_mem_vertex_usse_alloc(patcherVertexUsseSize, &data->patcherVertexUsseUid, &data->patcherVertexUsseOffset);

	data->patcherFragmentUsse = vita_mem_fragment_usse_alloc(
		patcherFragmentUsseSize,
		&data->patcherFragmentUsseUid,
		&data->patcherFragmentUsseOffset
	);

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

	if (SCE_ERR(sceGxmShaderPatcherCreate, &patcherParams, &data->shaderPatcher)) {
		return false;
	}
	return true;
}

static void destroy_gxm_context()
{
	sceGxmShaderPatcherDestroy(gxm_renderer_context.shaderPatcher);
	sceGxmDestroyContext(gxm_renderer_context.context);
	vita_mem_fragment_usse_free(gxm_renderer_context.fragmentUsseRingBufferUid);
	vita_mem_free(gxm_renderer_context.fragmentRingBufferUid);
	vita_mem_free(gxm_renderer_context.vertexRingBufferUid);
	vita_mem_free(gxm_renderer_context.vdmRingBufferUid);
	SDL_free(gxm_renderer_context.contextHostMem);
}

bool get_gxm_context(SceGxmContext** context, SceGxmShaderPatcher** shaderPatcher)
{
	if (!create_gxm_context()) {
		return false;
	}
	*context = gxm_renderer_context.context;
	*shaderPatcher = gxm_renderer_context.shaderPatcher;
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
	bool success = gxm_init();
	if (!success) {
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

	const unsigned int alignedWidth = ALIGN(m_width, SCE_GXM_TILE_SIZEX);
	const unsigned int alignedHeight = ALIGN(m_height, SCE_GXM_TILE_SIZEY);
	const unsigned int sampleCount = alignedWidth * alignedHeight;
	const unsigned int depthStrideInSamples = alignedWidth;

	if (!get_gxm_context(&this->context, &this->shaderPatcher)) {
		return;
	}

	if (!cdram_allocator_create()) {
		sceClibPrintf("failed to create cdram allocator");
		return;
	}

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
	if (SCE_ERR(sceGxmCreateRenderTarget, &renderTargetParams, &this->renderTarget)) {
		return;
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

		if (SCE_ERR(
				sceGxmColorSurfaceInit,
				&this->displayBuffersSurface[i],
				SCE_GXM_COLOR_FORMAT_A8B8G8R8,
				SCE_GXM_COLOR_SURFACE_LINEAR,
				SCE_GXM_COLOR_SURFACE_SCALE_NONE,
				SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
				m_width,
				m_height,
				m_width,
				this->displayBuffers[i]
			)) {
			return;
		}

		if (SCE_ERR(sceGxmSyncObjectCreate, &this->displayBuffersSync[i])) {
			return;
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
		4 * sampleCount,
		SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		&this->stencilBufferUid,
		"stencilBufferData"
	);

	if (SCE_ERR(
			sceGxmDepthStencilSurfaceInit,
			&this->depthSurface,
			SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24,
			SCE_GXM_DEPTH_STENCIL_SURFACE_TILED,
			depthStrideInSamples,
			this->depthBufferData,
			this->stencilBufferData
		)) {
		return;
	}

	// register shader programs
	if (SCE_ERR(
			sceGxmShaderPatcherRegisterProgram,
			this->shaderPatcher,
			colorFragmentProgramGxp,
			&this->colorFragmentProgramId
		)) {
		return;
	}
	if (SCE_ERR(
			sceGxmShaderPatcherRegisterProgram,
			this->shaderPatcher,
			mainVertexProgramGxp,
			&this->mainVertexProgramId
		)) {
		return;
	}
	if (SCE_ERR(
			sceGxmShaderPatcherRegisterProgram,
			this->shaderPatcher,
			mainColorFragmentProgramGxp,
			&this->mainColorFragmentProgramId
		)) {
		return;
	}
	if (SCE_ERR(
			sceGxmShaderPatcherRegisterProgram,
			this->shaderPatcher,
			mainTextureFragmentProgramGxp,
			&this->mainTextureFragmentProgramId
		)) {
		return;
	}
	if (SCE_ERR(
			sceGxmShaderPatcherRegisterProgram,
			this->shaderPatcher,
			imageFragmentProgramGxp,
			&this->imageFragmentProgramId
		)) {
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

		vertexStreams[0].stride = sizeof(Vertex);
		vertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

		if (SCE_ERR(
				sceGxmShaderPatcherCreateVertexProgram,
				this->shaderPatcher,
				this->mainVertexProgramId,
				vertexAttributes,
				3,
				vertexStreams,
				1,
				&this->mainVertexProgram
			)) {
			return;
		}
	}

	// main color opaque
	if (SCE_ERR(
			sceGxmShaderPatcherCreateFragmentProgram,
			this->shaderPatcher,
			this->mainColorFragmentProgramId,
			SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
			SCE_GXM_MULTISAMPLE_NONE,
			&blendInfoOpaque,
			mainVertexProgramGxp,
			&this->opaqueColorFragmentProgram
		)) {
		return;
	}

	// main color blended
	if (SCE_ERR(
			sceGxmShaderPatcherCreateFragmentProgram,
			this->shaderPatcher,
			this->mainColorFragmentProgramId,
			SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
			SCE_GXM_MULTISAMPLE_NONE,
			&blendInfoTransparent,
			mainVertexProgramGxp,
			&this->blendedColorFragmentProgram
		)) {
		return;
	}

	// main texture opaque
	if (SCE_ERR(
			sceGxmShaderPatcherCreateFragmentProgram,
			this->shaderPatcher,
			this->mainTextureFragmentProgramId,
			SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
			SCE_GXM_MULTISAMPLE_NONE,
			&blendInfoOpaque,
			mainVertexProgramGxp,
			&this->opaqueTextureFragmentProgram
		)) {
		return;
	}

	// main texture transparent
	if (SCE_ERR(
			sceGxmShaderPatcherCreateFragmentProgram,
			this->shaderPatcher,
			this->mainTextureFragmentProgramId,
			SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
			SCE_GXM_MULTISAMPLE_NONE,
			&blendInfoTransparent,
			mainVertexProgramGxp,
			&this->blendedTextureFragmentProgram
		)) {
		return;
	}

	// image
	if (SCE_ERR(
			sceGxmShaderPatcherCreateFragmentProgram,
			this->shaderPatcher,
			this->imageFragmentProgramId,
			SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
			SCE_GXM_MULTISAMPLE_NONE,
			&blendInfoTransparent,
			mainVertexProgramGxp,
			&this->imageFragmentProgram
		)) {
		return;
	}

	// color
	if (SCE_ERR(
			sceGxmShaderPatcherCreateFragmentProgram,
			this->shaderPatcher,
			this->colorFragmentProgramId,
			SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
			SCE_GXM_MULTISAMPLE_NONE,
			NULL,
			mainVertexProgramGxp,
			&this->colorFragmentProgram
		)) {
		return;
	}

	// vertex uniforms
	this->uModelViewMatrix = sceGxmProgramFindParameterByName(mainVertexProgramGxp, "uModelViewMatrix");
	this->uNormalMatrix = sceGxmProgramFindParameterByName(mainVertexProgramGxp, "uNormalMatrix");
	this->uProjectionMatrix = sceGxmProgramFindParameterByName(mainVertexProgramGxp, "uProjectionMatrix");

	// fragment uniforms
	this->color_uLights = sceGxmProgramFindParameterByName(mainColorFragmentProgramGxp, "uLights");         // SceneLight[3]
	this->color_uAmbientLight = sceGxmProgramFindParameterByName(mainColorFragmentProgramGxp, "uAmbientLight"); // vec3
	this->color_uShininess = sceGxmProgramFindParameterByName(mainColorFragmentProgramGxp, "uShininess");   // float
	this->color_uColor = sceGxmProgramFindParameterByName(mainColorFragmentProgramGxp, "uColor");           // vec4
	this->texture_uLights = sceGxmProgramFindParameterByName(mainTextureFragmentProgramGxp, "uLights");         // SceneLight[3]
	this->texture_uAmbientLight = sceGxmProgramFindParameterByName(mainTextureFragmentProgramGxp, "uAmbientLight"); // vec3
	this->texture_uShininess = sceGxmProgramFindParameterByName(mainTextureFragmentProgramGxp, "uShininess");   // float
	this->texture_uColor = sceGxmProgramFindParameterByName(mainTextureFragmentProgramGxp, "uColor");           // vec4

	// clear uniforms
	this->colorShader_uColor = sceGxmProgramFindParameterByName(colorFragmentProgramGxp, "uColor"); // vec4

	for (int i = 0; i < GXM_FRAGMENT_BUFFER_COUNT; i++) {
		this->lights[i] = static_cast<GXMSceneLightUniform*>(cdram_alloc(sizeof(GXMSceneLightUniform), 4));
	}
	for (int i = 0; i < GXM_VERTEX_BUFFER_COUNT; i++) {
		this->quadVertices[i] = static_cast<Vertex*>(cdram_alloc(sizeof(Vertex) * 4 * 50, 4));
	}
	this->quadIndices = static_cast<uint16_t*>(cdram_alloc(sizeof(uint16_t) * 4, 4));
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

	int count;
	auto ids = SDL_GetGamepads(&count);
	for (int i = 0; i < count; i++) {
		auto id = ids[i];
		auto gamepad = SDL_OpenGamepad(id);
		if (gamepad != nullptr) {
			this->gamepad = gamepad;
			break;
		}
	}

	m_initialized = true;
}

GXMRenderer::~GXMRenderer()
{
	if (!m_initialized) {
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
			cdram_free(textureData);
			cache.texture = nullptr;
			memset(&cache.gxmTexture, 0, sizeof(cache.gxmTexture));
			delete ctx;
		},
		ctx
	);
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

Uint32 GXMRenderer::GetTextureId(IDirect3DRMTexture* iTexture, bool isUi)
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
				void* textureData = sceGxmTextureGetData(&tex.gxmTexture);
				copySurfaceToGxm(surface, (uint8_t*) textureData, textureStride, textureSize);
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
	void* textureData = cdram_alloc(textureSize, textureAlignment);
	copySurfaceToGxm(surface, (uint8_t*) textureData, textureStride, textureSize);

	SceGxmTexture gxmTexture;
	SCE_ERR(sceGxmTextureInitLinear, &gxmTexture, textureData, gxmTextureFormat, textureWidth, textureHeight, 0);
	sceGxmTextureSetMinFilter(&gxmTexture, SCE_GXM_TEXTURE_FILTER_LINEAR);
	sceGxmTextureSetMagFilter(&gxmTexture, SCE_GXM_TEXTURE_FILTER_LINEAR);
	sceGxmTextureSetUAddrMode(&gxmTexture, SCE_GXM_TEXTURE_ADDR_REPEAT);
	sceGxmTextureSetVAddrMode(&gxmTexture, SCE_GXM_TEXTURE_ADDR_REPEAT);
	if (gxmTextureFormat == SCE_GXM_TEXTURE_FORMAT_P8_ABGR) {
		sceGxmTextureSetPalette(&gxmTexture, (uint8_t*) textureData + paletteOffset);
	}

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

	size_t vertexBufferSize = sizeof(Vertex) * vertices.size();
	size_t indexBufferSize = sizeof(uint16_t) * indices.size();
	void* meshData = cdram_alloc(vertexBufferSize + indexBufferSize, 4);

	Vertex* vertexBuffer = (Vertex*) meshData;
	uint16_t* indexBuffer = (uint16_t*) ((uint8_t*) meshData + vertexBufferSize);

	for (int i = 0; i < vertices.size(); i++) {
		D3DRMVERTEX vertex = vertices.data()[i];
		vertexBuffer[i] = Vertex{
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

#ifdef GXM_PRECOMPUTE
	bool isOpaque = meshGroup.color.a == 0xff;
	const SceGxmTexture* texture = nullptr;
	if(meshGroup.texture) {
		Uint32 textureId = GetTextureId(meshGroup.texture, false);
		texture = &this->m_textures[textureId].gxmTexture;
	}

	const SceGxmProgram* fragmentProgramGxp;
	const SceGxmFragmentProgram *fragmentProgram;
	if(texture) {
		fragmentProgramGxp = mainTextureFragmentProgramGxp;
		fragmentProgram = isOpaque ? this->opaqueTextureFragmentProgram : this->blendedTextureFragmentProgram;
	} else {
		fragmentProgramGxp = mainColorFragmentProgramGxp;
		fragmentProgram = isOpaque ? this->opaqueColorFragmentProgram : this->blendedColorFragmentProgram;
	}

	// get sizes
	const size_t drawSize = ALIGN(sceGxmGetPrecomputedDrawSize(this->mainVertexProgram), SCE_GXM_PRECOMPUTED_ALIGNMENT);
	const size_t vertexStateSize = ALIGN(sceGxmGetPrecomputedVertexStateSize(this->mainVertexProgram), SCE_GXM_PRECOMPUTED_ALIGNMENT);
	const size_t fragmentStateSize = ALIGN(sceGxmGetPrecomputedFragmentStateSize(fragmentProgram), SCE_GXM_PRECOMPUTED_ALIGNMENT);
	const size_t precomputeDataSize =
		drawSize +
		vertexStateSize * GXM_VERTEX_BUFFER_COUNT +
		fragmentStateSize * GXM_FRAGMENT_BUFFER_COUNT;

	const size_t vertexDefaultBufferSize = sceGxmProgramGetDefaultUniformBufferSize(mainVertexProgramGxp);
	const size_t fragmentDefaultBufferSize = sceGxmProgramGetDefaultUniformBufferSize(fragmentProgramGxp);
	const size_t uniformBuffersSize =
		vertexDefaultBufferSize * GXM_VERTEX_BUFFER_COUNT;
		fragmentStateSize * GXM_FRAGMENT_BUFFER_COUNT;

	sceClibPrintf("drawSize: %d\n", drawSize);
	sceClibPrintf("vertexStateSize: %d\n", vertexStateSize);
	sceClibPrintf("fragmentStateSize: %d\n", fragmentStateSize);
	sceClibPrintf("precomputeDataSize: %d\n", precomputeDataSize);
	sceClibPrintf("vertexDefaultBufferSize: %d\n", vertexDefaultBufferSize);
	sceClibPrintf("fragmentDefaultBufferSize: %d\n", fragmentDefaultBufferSize);
	sceClibPrintf("uniformBuffersSize: %d\n", uniformBuffersSize);

	// allocate the precompute buffer, combined for all
	uint8_t* precomputeData = (uint8_t*)cdram_alloc(precomputeDataSize, SCE_GXM_PRECOMPUTED_ALIGNMENT);
	cache.precomputeData = precomputeData;

	uint8_t* uniformBuffers = (uint8_t*)cdram_alloc(uniformBuffersSize, 4);
	cache.uniformBuffers = uniformBuffers;

	// init precomputed draw
	SCE_ERR(sceGxmPrecomputedDrawInit,
		&cache.drawState,
		this->mainVertexProgram,
		precomputeData
	);
	void* vertexStreams[] = { vertexBuffer };
	sceGxmPrecomputedDrawSetAllVertexStreams(&cache.drawState, vertexStreams);
	sceGxmPrecomputedDrawSetParams(
		&cache.drawState,
		SCE_GXM_PRIMITIVE_TRIANGLES,
		SCE_GXM_INDEX_FORMAT_U16,
		indexBuffer,
		indices.size()
	);
	precomputeData += drawSize;

	// init precomputed vertex state
	for(int bufferIndex = 0; bufferIndex < GXM_VERTEX_BUFFER_COUNT; bufferIndex++) {
		SCE_ERR(sceGxmPrecomputedVertexStateInit,
			&cache.vertexState[bufferIndex],
			this->mainVertexProgram,
			precomputeData
		);
		sceGxmPrecomputedVertexStateSetDefaultUniformBuffer(
			&cache.vertexState[bufferIndex],
			uniformBuffers
		);
		precomputeData += vertexStateSize;
		uniformBuffers += vertexDefaultBufferSize;
	}

	// init precomputed fragment state
	for(int bufferIndex = 0; bufferIndex < GXM_FRAGMENT_BUFFER_COUNT; bufferIndex++) {
		SCE_ERR(sceGxmPrecomputedFragmentStateInit,
			&cache.fragmentState[bufferIndex],
			fragmentProgram,
			precomputeData
		);
		if(texture) {
			sceGxmPrecomputedFragmentStateSetTexture(
				&cache.fragmentState[bufferIndex],
				0, texture
			);
		}
		sceGxmPrecomputedFragmentStateSetDefaultUniformBuffer(
			&cache.fragmentState[bufferIndex],
			uniformBuffers
		);
		precomputeData += fragmentStateSize;
		uniformBuffers += fragmentDefaultBufferSize;
	}
	assert(precomputeData - (uint8_t*)cache.precomputeData <= precomputeDataSize);
#endif
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
			cdram_free(cache.meshData);
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

bool razor_triggered = false;
bool razor_live_started = false;
bool razor_display_enabled = true;

void GXMRenderer::StartScene()
{
	if (sceneStarted) {
		return;
	}

	bool dpad_up = SDL_GetGamepadButton(this->gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP);
	bool dpad_down = SDL_GetGamepadButton(this->gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
	bool dpad_left = SDL_GetGamepadButton(this->gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
	bool dpad_right = SDL_GetGamepadButton(this->gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);

	// hud display
	if (with_razor_hud && dpad_up != this->button_dpad_up) {
		this->button_dpad_up = dpad_up;
		if (dpad_up) {
			sceRazorHudSetDisplayEnabled(razor_display_enabled);
		}
	}

	// capture frame
	if (with_razor && dpad_down != this->button_dpad_down) {
		this->button_dpad_down = dpad_down;
		if (dpad_down) {
			sceRazorGpuCaptureSetTriggerNextFrame("ux0:/data/capture.sgx");
			SDL_Log("trigger razor");
		}
	}

	// toggle live
	if (with_razor_hud && dpad_left != this->button_dpad_left) {
		this->button_dpad_left = dpad_left;
		if (dpad_left) {
			if (razor_live_started) {
				sceRazorGpuLiveStop();
				razor_live_started = false;
			}
			else {
				sceRazorGpuLiveStart();
				razor_live_started = true;
			}
		}
	}

	// trigger trace
	if (with_razor_hud && dpad_right != this->button_dpad_right) {
		this->button_dpad_right = dpad_right;
		if (dpad_right) {
			sceRazorGpuTraceTrigger();
		}
	}

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

	sceGxmNotificationWait(&this->vertexNotifications[this->currentVertexBufferIndex]);
	sceGxmNotificationWait(&this->fragmentNotifications[this->currentFragmentBufferIndex]);
}

HRESULT GXMRenderer::BeginFrame()
{
	this->transparencyEnabled = false;
	this->StartScene();

	auto lightData = this->LightsBuffer();
	int i = 0;
	for(const auto& light : m_lights) {
		if(!light.directional && !light.positional) {
			lightData->ambientLight[0] = light.color.r;
			lightData->ambientLight[1] = light.color.g;
			lightData->ambientLight[2] = light.color.b;
			continue;
		}
		if(i == 2) {
			sceClibPrintf("light overflow\n");
			continue;
		}

		lightData->lights[i].color[0] = light.color.r;
		lightData->lights[i].color[1] = light.color.g;
		lightData->lights[i].color[2] = light.color.b;
		lightData->lights[i].color[3] = light.color.a;

		lightData->lights[i].position[0] = light.position.x;
		lightData->lights[i].position[1] = light.position.y;
		lightData->lights[i].position[2] = light.position.z;
		lightData->lights[i].position[3] = light.positional;

		lightData->lights[i].direction[0] = light.direction.x;
		lightData->lights[i].direction[1] = light.direction.y;
		lightData->lights[i].direction[2] = light.direction.z;
		lightData->lights[i].direction[3] = light.directional;
		i++;
	}
	sceGxmSetFragmentUniformBuffer(this->context, 0, lightData);

	return DD_OK;
}

void GXMRenderer::EnableTransparency()
{
	this->transparencyEnabled = true;
}

static void transpose4x4(const float src[4][4], float dst[4][4])
{
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			dst[j][i] = src[i][j];
		}
	}
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
)
{
	auto& mesh = m_meshes[meshId];

	char marker[256];
	snprintf(marker, sizeof(marker), "SubmitDraw: %d", meshId);
	sceGxmPushUserMarker(this->context, marker);

	sceGxmSetVertexProgram(this->context, this->mainVertexProgram);

	bool textured = appearance.textureId != NO_TEXTURE_ID;

	const SceGxmFragmentProgram *fragmentProgram;
	if(this->transparencyEnabled) {
		fragmentProgram = textured ? this->blendedTextureFragmentProgram : this->blendedColorFragmentProgram;
	} else {
		fragmentProgram = textured ? this->opaqueTextureFragmentProgram : this->opaqueColorFragmentProgram;
	}
	sceGxmSetFragmentProgram(this->context, fragmentProgram);

	const SceGxmProgramParameter* uColor = textured ? this->texture_uColor : this->color_uColor;
	const SceGxmProgramParameter* uShininess = textured ? this->texture_uShininess : this->color_uShininess;

	float color[4] = {
		appearance.color.r / 255.0f,
		appearance.color.g / 255.0f,
		appearance.color.b / 255.0f,
		appearance.color.a / 255.0f
	};

	void* vertUniforms;
	void* fragUniforms;
#ifdef GXM_PRECOMPUTE
	sceGxmSetPrecomputedVertexState(this->context, &mesh.vertexState[this->currentVertexBufferIndex]);
	sceGxmSetPrecomputedFragmentState(this->context, &mesh.fragmentState[this->currentFragmentBufferIndex]);
	vertUniforms = sceGxmPrecomputedVertexStateGetDefaultUniformBuffer(&mesh.vertexState[this->currentVertexBufferIndex]);
	fragUniforms = sceGxmPrecomputedFragmentStateGetDefaultUniformBuffer(&mesh.fragmentState[this->currentFragmentBufferIndex]);
#else
	sceGxmReserveVertexDefaultUniformBuffer(this->context, &vertUniforms);
	sceGxmReserveFragmentDefaultUniformBuffer(this->context, &fragUniforms);
#endif

	SET_UNIFORM(vertUniforms, this->uModelViewMatrix, modelViewMatrix);
	SET_UNIFORM(vertUniforms, this->uProjectionMatrix, m_projection);
	sceGxmSetUniformDataF(vertUniforms, this->uNormalMatrix, 0, 9, static_cast<const float*>(normalMatrix[0]));

	SET_UNIFORM(fragUniforms, uColor, color);
	SET_UNIFORM(fragUniforms, uShininess, appearance.shininess);

	if (textured) {
		auto& texture = m_textures[appearance.textureId];
		sceGxmSetFragmentTexture(this->context, 0, &texture.gxmTexture);
	}

#ifdef GXM_PRECOMPUTE
	sceGxmDrawPrecomputed(this->context, &mesh.drawState);
	sceGxmSetPrecomputedVertexState(this->context, NULL);
	sceGxmSetPrecomputedFragmentState(this->context, NULL);
#else
	sceGxmSetVertexStream(this->context, 0, mesh.vertexBuffer);
	sceGxmDraw(this->context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, mesh.indexBuffer, mesh.indexCount);
#endif
	sceGxmPopUserMarker(this->context);
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
	SDL_Log("GXMRenderer::Resize TODO");
}

void GXMRenderer::Clear(float r, float g, float b)
{
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
	SET_UNIFORM(vertUniforms, this->uNormalMatrix, normal);         // float3x3
	SET_UNIFORM(vertUniforms, this->uProjectionMatrix, projection); // float4x4

	float color[] = {r, g, b, 1};
	SET_UNIFORM(fragUniforms, this->colorShader_uColor, color);

	float x1 = 0;
	float y1 = 0;
	float x2 = x1 + 1.0;
	float y2 = y1 + 1.0;

	Vertex* quadVertices = this->QuadVerticesBuffer();
	quadVertices[0] = Vertex{.position = {x1, y1, -1.0}, .normal = {0, 0, 0}, .texCoord = {0, 0}};
	quadVertices[1] = Vertex{.position = {x2, y1, -1.0}, .normal = {0, 0, 0}, .texCoord = {0, 0}};
	quadVertices[2] = Vertex{.position = {x1, y2, -1.0}, .normal = {0, 0, 0}, .texCoord = {0, 0}};
	quadVertices[3] = Vertex{.position = {x2, y2, -1.0}, .normal = {0, 0, 0}, .texCoord = {0, 0}};

	sceGxmSetVertexStream(this->context, 0, quadVertices);
	sceGxmDraw(this->context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, this->quadIndices, 4);

	sceGxmPopUserMarker(this->context);
}

void GXMRenderer::Flip()
{
	if (!this->sceneStarted) {
		this->Clear(0, 0, 0);
	}

	++this->vertexNotifications[this->currentVertexBufferIndex].value;
	++this->fragmentNotifications[this->currentFragmentBufferIndex].value;
	sceGxmEndScene(
		this->context,
		&this->vertexNotifications[this->currentVertexBufferIndex],
		&this->fragmentNotifications[this->currentFragmentBufferIndex]
	);

	this->currentVertexBufferIndex = (this->currentVertexBufferIndex + 1) % GXM_VERTEX_BUFFER_COUNT;
	this->currentFragmentBufferIndex = (this->currentFragmentBufferIndex + 1) % GXM_FRAGMENT_BUFFER_COUNT;

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
	this->backBufferIndex = (this->backBufferIndex + 1) % GXM_DISPLAY_BUFFER_COUNT;
}

void GXMRenderer::Draw2DImage(Uint32 textureId, const SDL_Rect& srcRect, const SDL_Rect& dstRect)
{
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

	SET_UNIFORM(vertUniforms, this->uModelViewMatrix, identity);    // float4x4
	SET_UNIFORM(vertUniforms, this->uNormalMatrix, normal);         // float3x3
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
	quadVertices[0] = Vertex{.position = {x1, y1, 0}, .normal = {0, 0, 0}, .texCoord = {u1, v1}};
	quadVertices[1] = Vertex{.position = {x2, y1, 0}, .normal = {0, 0, 0}, .texCoord = {u2, v1}};
	quadVertices[2] = Vertex{.position = {x1, y2, 0}, .normal = {0, 0, 0}, .texCoord = {u1, v2}};
	quadVertices[3] = Vertex{.position = {x2, y2, 0}, .normal = {0, 0, 0}, .texCoord = {u2, v2}};

	sceGxmSetVertexStream(this->context, 0, quadVertices);

	sceGxmSetFrontDepthWriteEnable(this->context, SCE_GXM_DEPTH_WRITE_DISABLED);
	sceGxmDraw(this->context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, this->quadIndices, 4);
	sceGxmSetFrontDepthWriteEnable(this->context, SCE_GXM_DEPTH_WRITE_ENABLED);
	sceGxmPopUserMarker(this->context);
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
		this->m_width,
		this->m_height,
		SDL_PIXELFORMAT_RGBA32,
		this->displayBuffers[this->frontBufferIndex],
		VITA_GXM_SCREEN_STRIDE
	);
	SDL_BlitSurfaceScaled(src, &srcRect, target, nullptr, SDL_SCALEMODE_NEAREST);
	SDL_DestroySurface(src);
}
