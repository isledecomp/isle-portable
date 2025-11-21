#include "ShaderIndex.h"
#include "d3drmrenderer.h"
#include "d3drmrenderer_sdl3gpu_priv.h"
#include "ddraw_impl.h"
#include "mathutils.h"
#include "meshutils.h"
#include "miniwin.h"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <mortar/backends/sdl3_dynamic.h>
#include <mortar/mortar.h>

static SDL_Window* to_SDL3_Window(MORTAR_Window* mortar_window)
{
	SDL_Window* sdl3_window =
		(SDL_Window*) MORTAR_EXT_GetWindowProperty(mortar_window, MORTAR_WINDOW_PROPERTY_SDL3WINDOW, nullptr);
	SDL_assert(sdl3_window != NULL);
	return sdl3_window;
}

static SDL_Color to_SDL_Color(const MORTAR_Color& mortar_color)
{
	SDL_Color sdl_color;
	sdl_color.r = mortar_color.r;
	sdl_color.g = mortar_color.g;
	sdl_color.b = mortar_color.b;
	sdl_color.a = mortar_color.a;
	return sdl_color;
}

struct ScopedSurface {
	SDL_Surface* ptr = nullptr;
	~ScopedSurface()
	{
		if (ptr) {
			SDL_DestroySurface(ptr);
		}
	}
	void release() { ptr = nullptr; }
};

struct ScopedTexture {
	SDL_GPUDevice* dev;
	SDL_GPUTexture* ptr = nullptr;
	~ScopedTexture()
	{
		if (ptr) {
			SDL_ReleaseGPUTexture(dev, ptr);
		}
	}
	void release() { ptr = nullptr; }
};

struct ScopedTransferBuffer {
	SDL_GPUDevice* dev;
	SDL_GPUTransferBuffer* ptr = nullptr;
	~ScopedTransferBuffer()
	{
		if (ptr) {
			SDL_ReleaseGPUTransferBuffer(dev, ptr);
		}
	}
	void release() { ptr = nullptr; }
};

struct ScopedDevice {
	SDL_GPUDevice* ptr = nullptr;
	~ScopedDevice()
	{
		if (ptr) {
			SDL_DestroyGPUDevice(ptr);
		}
	}
	void release() { ptr = nullptr; }
};

struct ScopedPipeline {
	SDL_GPUDevice* dev;
	SDL_GPUGraphicsPipeline* ptr = nullptr;
	~ScopedPipeline()
	{
		if (ptr) {
			SDL_ReleaseGPUGraphicsPipeline(dev, ptr);
		}
	}
	void release() { ptr = nullptr; }
};

struct ScopedSampler {
	SDL_GPUDevice* dev;
	SDL_GPUSampler* ptr = nullptr;
	~ScopedSampler()
	{
		if (ptr) {
			SDL_ReleaseGPUSampler(dev, ptr);
		}
	}
	void release() { ptr = nullptr; }
};

struct ScopedShader {
	SDL_GPUDevice* dev;
	SDL_GPUShader* ptr = nullptr;
	~ScopedShader()
	{
		if (ptr) {
			SDL_ReleaseGPUShader(dev, ptr);
		}
	}
	void release() { ptr = nullptr; }
};

static SDL_GPUGraphicsPipeline* InitializeGraphicsPipeline(
	SDL_GPUDevice* device,
	MORTAR_Window* window,
	bool depthTest,
	bool depthWrite
)
{
	const SDL_GPUShaderCreateInfo* vertexCreateInfo =
		GetVertexShaderCode(VertexShaderId::PositionColor, SDL_GetGPUShaderFormats(device));
	if (!vertexCreateInfo) {
		return nullptr;
	}
	ScopedShader vertexShader{device, SDL_CreateGPUShader(device, vertexCreateInfo)};
	if (!vertexShader.ptr) {
		return nullptr;
	}

	const SDL_GPUShaderCreateInfo* fragmentCreateInfo =
		GetFragmentShaderCode(FragmentShaderId::SolidColor, SDL_GetGPUShaderFormats(device));
	if (!fragmentCreateInfo) {
		return nullptr;
	}
	ScopedShader fragmentShader{device, SDL_CreateGPUShader(device, fragmentCreateInfo)};
	if (!fragmentShader.ptr) {
		return nullptr;
	}

	SDL_GPUVertexBufferDescription vertexBufferDescs[1] = {};
	vertexBufferDescs[0].slot = 0;
	vertexBufferDescs[0].pitch = sizeof(D3DRMVERTEX);
	vertexBufferDescs[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

	SDL_GPUVertexAttribute vertexAttrs[3] = {};
	vertexAttrs[0].location = 0;
	vertexAttrs[0].buffer_slot = 0;
	vertexAttrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	vertexAttrs[0].offset = offsetof(D3DRMVERTEX, position);

	vertexAttrs[1].location = 1;
	vertexAttrs[1].buffer_slot = 0;
	vertexAttrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	vertexAttrs[1].offset = offsetof(D3DRMVERTEX, normal);

	vertexAttrs[2].location = 2;
	vertexAttrs[2].buffer_slot = 0;
	vertexAttrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
	vertexAttrs[2].offset = offsetof(D3DRMVERTEX, texCoord);

	SDL_GPUVertexInputState vertexInputState = {};
	vertexInputState.vertex_buffer_descriptions = vertexBufferDescs;
	vertexInputState.num_vertex_buffers = SDL_arraysize(vertexBufferDescs);
	vertexInputState.vertex_attributes = vertexAttrs;
	vertexInputState.num_vertex_attributes = SDL_arraysize(vertexAttrs);

	SDL_GPUColorTargetDescription colorTargets = {};
	colorTargets.format = SDL_GetGPUSwapchainTextureFormat(device, to_SDL3_Window(window));
	if (depthTest && depthWrite) {
		colorTargets.blend_state.enable_blend = false;
	}
	else {
		colorTargets.blend_state.enable_blend = true;
		colorTargets.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
		colorTargets.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		colorTargets.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
		colorTargets.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
		colorTargets.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
		colorTargets.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
		colorTargets.blend_state.enable_color_write_mask = false;
	}

	SDL_GPURasterizerState rasterizerState = {};
	rasterizerState.fill_mode = SDL_GPU_FILLMODE_FILL;
	rasterizerState.cull_mode = SDL_GPU_CULLMODE_BACK;
	rasterizerState.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;
	rasterizerState.enable_depth_clip = true;

	SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.vertex_shader = vertexShader.ptr;
	pipelineCreateInfo.fragment_shader = fragmentShader.ptr;
	pipelineCreateInfo.vertex_input_state = vertexInputState;
	pipelineCreateInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	pipelineCreateInfo.rasterizer_state = rasterizerState;
	pipelineCreateInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_GREATER;
	pipelineCreateInfo.depth_stencil_state.write_mask = 0xff;
	pipelineCreateInfo.depth_stencil_state.enable_depth_test = depthTest;
	pipelineCreateInfo.depth_stencil_state.enable_depth_write = depthWrite;
	pipelineCreateInfo.depth_stencil_state.enable_stencil_test = false;
	pipelineCreateInfo.target_info.color_target_descriptions = &colorTargets;
	pipelineCreateInfo.target_info.num_color_targets = 1;
	pipelineCreateInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
	pipelineCreateInfo.target_info.has_depth_stencil_target = true;

	return SDL_CreateGPUGraphicsPipeline(device, &pipelineCreateInfo);
}

Direct3DRMRenderer* Direct3DRMSDL3GPURenderer::Create(DWORD width, DWORD height)
{
	ScopedDevice device{SDL_CreateGPUDevice(
		SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXBC | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL,
#ifdef DEBUG
		true,
#else
		false,
#endif
		nullptr
	)};
	if (!device.ptr) {
		MORTAR_LogError(LOG_CATEGORY_MINIWIN, "SDL_CreateGPUDevice failed (%s)", MORTAR_GetError());
		return nullptr;
	}

	if (!DDWindow) {
		MORTAR_Log("No window handler");
		return nullptr;
	}

	if (!SDL_ClaimWindowForGPUDevice(device.ptr, to_SDL3_Window(DDWindow))) {
		MORTAR_LogError(LOG_CATEGORY_MINIWIN, "SDL_ClaimWindowForGPUDevice: %s", MORTAR_GetError());
		return nullptr;
	}

	ScopedPipeline opaquePipeline{device.ptr, InitializeGraphicsPipeline(device.ptr, DDWindow, true, true)};
	if (!opaquePipeline.ptr) {
		MORTAR_LogError(LOG_CATEGORY_MINIWIN, "InitializeGraphicsPipeline for opaquePipeline");
		return nullptr;
	}

	ScopedPipeline transparentPipeline{device.ptr, InitializeGraphicsPipeline(device.ptr, DDWindow, true, false)};
	if (!transparentPipeline.ptr) {
		MORTAR_LogError(LOG_CATEGORY_MINIWIN, "InitializeGraphicsPipeline for transparentPipeline");
		return nullptr;
	}

	ScopedPipeline uiPipeline{device.ptr, InitializeGraphicsPipeline(device.ptr, DDWindow, false, false)};
	if (!uiPipeline.ptr) {
		MORTAR_LogError(LOG_CATEGORY_MINIWIN, "InitializeGraphicsPipeline for uiPipeline");
		return nullptr;
	}

	// Setup texture CPU-to-GPU transfer
	SDL_GPUTransferBufferCreateInfo uploadBufferInfo = {};
	uploadBufferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	int uploadBufferSize = 640 * 480 * 4; // Largest known game texture
	uploadBufferInfo.size = uploadBufferSize;
	ScopedTransferBuffer uploadBuffer{device.ptr, SDL_CreateGPUTransferBuffer(device.ptr, &uploadBufferInfo)};
	if (!uploadBuffer.ptr) {
		MORTAR_LogError(
			LOG_CATEGORY_MINIWIN,
			"SDL_CreateGPUTransferBuffer filed for upload buffer (%s)",
			MORTAR_GetError()
		);
		return nullptr;
	}

	SDL_GPUSamplerCreateInfo samplerInfo = {};
	samplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
	samplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
	samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
	samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
	samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
	samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
	ScopedSampler sampler{device.ptr, SDL_CreateGPUSampler(device.ptr, &samplerInfo)};
	if (!sampler.ptr) {
		MORTAR_LogError(LOG_CATEGORY_MINIWIN, "Failed to create sampler: %s", MORTAR_GetError());
		return nullptr;
	}

	SDL_GPUSamplerCreateInfo uiSamplerInfo = {};
	uiSamplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
	uiSamplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
	uiSamplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
	uiSamplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	uiSamplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	uiSamplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	ScopedSampler uiSampler{device.ptr, SDL_CreateGPUSampler(device.ptr, &uiSamplerInfo)};
	if (!uiSampler.ptr) {
		MORTAR_LogError(LOG_CATEGORY_MINIWIN, "Failed to create sampler: %s", MORTAR_GetError());
		return nullptr;
	}

	auto renderer = new Direct3DRMSDL3GPURenderer(
		width,
		height,
		device.ptr,
		opaquePipeline.ptr,
		transparentPipeline.ptr,
		uiPipeline.ptr,
		sampler.ptr,
		uiSampler.ptr,
		uploadBuffer.ptr,
		uploadBufferSize
	);

	// Release resources so they don't get cleaned up
	device.release();
	opaquePipeline.release();
	transparentPipeline.release();
	uiPipeline.release();
	sampler.release();
	uiSampler.release();
	uploadBuffer.release();

	return renderer;
}

Direct3DRMSDL3GPURenderer::Direct3DRMSDL3GPURenderer(
	DWORD width,
	DWORD height,
	SDL_GPUDevice* device,
	SDL_GPUGraphicsPipeline* opaquePipeline,
	SDL_GPUGraphicsPipeline* transparentPipeline,
	SDL_GPUGraphicsPipeline* uiPipeline,
	SDL_GPUSampler* sampler,
	SDL_GPUSampler* uiSampler,
	SDL_GPUTransferBuffer* uploadBuffer,
	int uploadBufferSize
)
	: m_device(device), m_opaquePipeline(opaquePipeline), m_transparentPipeline(transparentPipeline),
	  m_uiPipeline(uiPipeline), m_sampler(sampler), m_uiSampler(uiSampler), m_uploadBuffer(uploadBuffer),
	  m_uploadBufferSize(uploadBufferSize)
{
	m_width = width;
	m_height = height;
	m_virtualWidth = width;
	m_virtualHeight = height;
	SDL_Surface* dummySurface = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGBA32);
	if (!dummySurface) {
		MORTAR_LogError(LOG_CATEGORY_MINIWIN, "Failed to create surface: %s", MORTAR_GetError());
		return;
	}
	if (!SDL_LockSurface(dummySurface)) {
		MORTAR_LogError(LOG_CATEGORY_MINIWIN, "Failed to lock surface: %s", MORTAR_GetError());
		SDL_DestroySurface(dummySurface);
		return;
	}
	((Uint32*) dummySurface->pixels)[0] = 0xFFFFFFFF;
	SDL_UnlockSurface(dummySurface);

	m_dummyTexture = CreateTextureFromSurface(dummySurface);
	if (!m_dummyTexture) {
		SDL_DestroySurface(dummySurface);
		MORTAR_LogError(LOG_CATEGORY_MINIWIN, "Failed to create surface: %s", MORTAR_GetError());
		return;
	}
	SDL_DestroySurface(dummySurface);

	ViewportTransform viewportTransform = {1.0f, 0.0f, 0.0f};
	Resize(m_width, m_height, viewportTransform);

	m_uiMesh.vertices = {
		{{0.0f, 0.0f, 0.0f}, {0, 0, -1}, {0.0f, 0.0f}},
		{{1.0f, 0.0f, 0.0f}, {0, 0, -1}, {1.0f, 0.0f}},
		{{1.0f, 1.0f, 0.0f}, {0, 0, -1}, {1.0f, 1.0f}},
		{{0.0f, 1.0f, 0.0f}, {0, 0, -1}, {0.0f, 1.0f}}
	};
	m_uiMesh.indices = {0, 1, 2, 0, 2, 3};
	m_uiMeshCache = UploadMesh(m_uiMesh);
}

Direct3DRMSDL3GPURenderer::~Direct3DRMSDL3GPURenderer()
{
	assert(m_refCount == 0);
	SDL_ReleaseGPUBuffer(m_device, m_uiMeshCache.vertexBuffer);
	SDL_ReleaseGPUBuffer(m_device, m_uiMeshCache.indexBuffer);
	SDL_ReleaseWindowFromGPUDevice(m_device, to_SDL3_Window(DDWindow));
	if (m_downloadBuffer) {
		SDL_ReleaseGPUTransferBuffer(m_device, m_downloadBuffer);
	}
	if (m_uploadBuffer) {
		SDL_ReleaseGPUTransferBuffer(m_device, m_uploadBuffer);
	}
	SDL_ReleaseGPUSampler(m_device, m_sampler);
	SDL_ReleaseGPUSampler(m_device, m_uiSampler);
	if (m_dummyTexture) {
		SDL_ReleaseGPUTexture(m_device, m_dummyTexture);
	}
	if (m_depthTexture) {
		SDL_ReleaseGPUTexture(m_device, m_depthTexture);
	}
	if (m_transferTexture) {
		SDL_ReleaseGPUTexture(m_device, m_transferTexture);
	}
	SDL_ReleaseGPUGraphicsPipeline(m_device, m_opaquePipeline);
	SDL_ReleaseGPUGraphicsPipeline(m_device, m_transparentPipeline);
	SDL_ReleaseGPUGraphicsPipeline(m_device, m_uiPipeline);
	if (m_uploadFence) {
		SDL_ReleaseGPUFence(m_device, m_uploadFence);
	}
	SDL_DestroyGPUDevice(m_device);
}

void Direct3DRMSDL3GPURenderer::PushLights(const SceneLight* lights, size_t count)
{
	if (count > 3) {
		MORTAR_LogError(LOG_CATEGORY_MINIWIN, "Unsupported number of lights (%d)", static_cast<int>(count));
		count = 3;
	}
	int lightCount = std::min(static_cast<int>(count), 3);
	memcpy(&m_fragmentShadingData.lights, lights, sizeof(SceneLight) * lightCount);
	m_fragmentShadingData.lightCount = lightCount;
}

void Direct3DRMSDL3GPURenderer::SetFrustumPlanes(const Plane* frustumPlanes)
{
}

void Direct3DRMSDL3GPURenderer::SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back)
{
	m_front = front;
	m_back = back;
	memcpy(&m_projection, projection, sizeof(D3DRMMATRIX4D));
}

void Direct3DRMSDL3GPURenderer::WaitForPendingUpload()
{
	if (!m_uploadFence) {
		return;
	}
	bool success = SDL_WaitForGPUFences(m_device, true, &m_uploadFence, 1);
	SDL_ReleaseGPUFence(m_device, m_uploadFence);
	m_uploadFence = nullptr;
	if (!success) {
		return;
	}
}

struct SDLTextureDestroyContext {
	Direct3DRMSDL3GPURenderer* renderer;
	Uint32 id;
};

void Direct3DRMSDL3GPURenderer::AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture)
{
	auto* ctx = new SDLTextureDestroyContext{this, id};
	texture->AddDestroyCallback(
		[](IDirect3DRMObject*, void* arg) {
			auto* ctx = static_cast<SDLTextureDestroyContext*>(arg);
			auto& cache = ctx->renderer->m_textures[ctx->id];
			if (cache.gpuTexture) {
				SDL_ReleaseGPUTexture(ctx->renderer->m_device, cache.gpuTexture);
				cache.gpuTexture = nullptr;
				cache.texture = nullptr;
			}
			delete ctx;
		},
		ctx
	);
}

SDL_GPUTexture* Direct3DRMSDL3GPURenderer::CreateTextureFromSurface(SDL_Surface* surface)
{
	ScopedSurface surf{SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32)};
	if (!surf.ptr) {
		MORTAR_LogError(LOG_CATEGORY_MINIWIN, "SDL_ConvertSurface (%s)", MORTAR_GetError());
		return nullptr;
	}

	const Uint32 dataSize = surf.ptr->pitch * surf.ptr->h;

	SDL_GPUTextureCreateInfo textureInfo = {};
	textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
	textureInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	textureInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
	textureInfo.width = surf.ptr->w;
	textureInfo.height = surf.ptr->h;
	textureInfo.layer_count_or_depth = 1;
	textureInfo.num_levels = 1;
	ScopedTexture texture{m_device, SDL_CreateGPUTexture(m_device, &textureInfo)};
	if (!texture.ptr) {
		MORTAR_LogError(LOG_CATEGORY_MINIWIN, "SDL_CreateGPUTexture (%s)", MORTAR_GetError());
		return nullptr;
	}
	SDL_GPUTransferBuffer* transferBuffer = GetUploadBuffer(dataSize);

	void* transferData = SDL_MapGPUTransferBuffer(m_device, transferBuffer, false);
	if (!transferData) {
		MORTAR_LogError(LOG_CATEGORY_MINIWIN, "SDL_MapGPUTransferBuffer (%s)", MORTAR_GetError());
		return nullptr;
	}
	memcpy(transferData, surf.ptr->pixels, dataSize);
	SDL_UnmapGPUTransferBuffer(m_device, transferBuffer);

	SDL_GPUTextureTransferInfo transferRegionInfo = {};
	transferRegionInfo.transfer_buffer = transferBuffer;
	SDL_GPUTextureRegion textureRegion = {};
	textureRegion.texture = texture.ptr;
	textureRegion.w = surf.ptr->w;
	textureRegion.h = surf.ptr->h;
	textureRegion.d = 1;

	SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(m_device);
	if (!cmdbuf) {
		MORTAR_LogError(
			LOG_CATEGORY_MINIWIN,
			"SDL_AcquireGPUCommandBuffer in CreateTextureFromSurface failed (%s)",
			MORTAR_GetError()
		);
		return nullptr;
	}
	SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(cmdbuf);
	SDL_UploadToGPUTexture(pass, &transferRegionInfo, &textureRegion, false);
	SDL_EndGPUCopyPass(pass);
	m_uploadFence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmdbuf);

	auto texptr = texture.ptr;

	// Release texture ownership so caller can manage it safely
	texture.release();

	return texptr;
}

uint32_t Direct3DRMSDL3GPURenderer::GetTextureId(IDirect3DRMTexture* iTexture, bool isUI, float scaleX, float scaleY)
{
	auto texture = static_cast<Direct3DRMTextureImpl*>(iTexture);
	auto surface = static_cast<DirectDrawSurfaceImpl*>(texture->m_surface);
	SDL_Surface* surf =
		(SDL_Surface*) MORTAR_EXT_GetSurfaceProperty(surface->m_surface, MORTAR_SURFACE_PROPERTY_SDL3SURFACE, nullptr);

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (tex.texture == texture) {
			if (tex.version != texture->m_version) {
				SDL_ReleaseGPUTexture(m_device, tex.gpuTexture);
				tex.gpuTexture = CreateTextureFromSurface(surf);
				if (!tex.gpuTexture) {
					return NO_TEXTURE_ID;
				}
				tex.version = texture->m_version;
			}
			return i;
		}
	}

	SDL_GPUTexture* newTex = CreateTextureFromSurface(surf);
	if (!newTex) {
		return NO_TEXTURE_ID;
	}

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (!tex.texture) {
			tex = {texture, texture->m_version, newTex};
			AddTextureDestroyCallback(i, texture);
			return i;
		}
	}

	m_textures.push_back({texture, texture->m_version, newTex});
	AddTextureDestroyCallback((Uint32) (m_textures.size() - 1), texture);
	return (Uint32) (m_textures.size() - 1);
}

SDL3MeshCache Direct3DRMSDL3GPURenderer::UploadMesh(const MeshGroup& meshGroup)
{
	std::vector<D3DRMVERTEX> finalVertices;
	std::vector<Uint16> finalIndices;

	if (meshGroup.quality == D3DRMRENDER_FLAT || meshGroup.quality == D3DRMRENDER_UNLITFLAT) {
		std::vector<uint16_t> newIndices;
		FlattenSurfaces(
			meshGroup.vertices.data(),
			meshGroup.vertices.size(),
			meshGroup.indices.data(),
			meshGroup.indices.size(),
			true,
			finalVertices,
			newIndices
		);

		finalIndices.assign(newIndices.begin(), newIndices.end());
	}
	else {
		finalVertices = meshGroup.vertices;
		finalIndices.assign(meshGroup.indices.begin(), meshGroup.indices.end());
	}

	SDL_GPUBufferCreateInfo vertexBufferCreateInfo = {};
	vertexBufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
	vertexBufferCreateInfo.size = sizeof(D3DRMVERTEX) * finalVertices.size();
	SDL_GPUBuffer* vertexBuffer = SDL_CreateGPUBuffer(m_device, &vertexBufferCreateInfo);
	if (!vertexBuffer) {
		MORTAR_LogError(LOG_CATEGORY_MINIWIN, "SDL_CreateGPUBuffer failed (%s)", MORTAR_GetError());
		return {};
	}

	SDL_GPUBufferCreateInfo indexBufferCreateInfo = {};
	indexBufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
	indexBufferCreateInfo.size = sizeof(Uint16) * finalIndices.size();
	SDL_GPUBuffer* indexBuffer = SDL_CreateGPUBuffer(m_device, &indexBufferCreateInfo);
	if (!indexBuffer) {
		MORTAR_LogError(LOG_CATEGORY_MINIWIN, "Failed to create index buffer (%s)", MORTAR_GetError());
		return {};
	}

	size_t verticesSize = sizeof(D3DRMVERTEX) * finalVertices.size();
	size_t indicesSize = sizeof(Uint16) * finalIndices.size();

	SDL_GPUTransferBuffer* uploadBuffer = GetUploadBuffer(verticesSize + indicesSize);
	if (!uploadBuffer) {
		return {};
	}
	auto* transferData = static_cast<uint8_t*>(SDL_MapGPUTransferBuffer(m_device, uploadBuffer, false));
	if (!transferData) {
		MORTAR_LogError(LOG_CATEGORY_MINIWIN, "Transfer buffer mapping failed (%s)", MORTAR_GetError());
		return {};
	}
	memcpy(transferData, finalVertices.data(), verticesSize);
	memcpy(transferData + verticesSize, finalIndices.data(), indicesSize);
	SDL_UnmapGPUTransferBuffer(m_device, uploadBuffer);

	SDL_GPUTransferBufferLocation transferLocation = {uploadBuffer};

	// Upload vertex + index data to GPU
	SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(m_device);
	if (!cmdbuf) {
		MORTAR_LogError(LOG_CATEGORY_MINIWIN, "SDL_AcquireGPUCommandBuffer failed (%s)", MORTAR_GetError());
		return {};
	}

	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdbuf);

	SDL_GPUBufferRegion vertexRegion = {};
	vertexRegion.buffer = vertexBuffer;
	vertexRegion.size = verticesSize;
	SDL_UploadToGPUBuffer(copyPass, &transferLocation, &vertexRegion, false);

	SDL_GPUTransferBufferLocation indexTransferLocation = {};
	indexTransferLocation.transfer_buffer = uploadBuffer;
	indexTransferLocation.offset = verticesSize;
	SDL_GPUBufferRegion indexRegion = {};
	indexRegion.buffer = indexBuffer;
	indexRegion.size = indicesSize;
	SDL_UploadToGPUBuffer(copyPass, &indexTransferLocation, &indexRegion, false);

	SDL_EndGPUCopyPass(copyPass);
	m_uploadFence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmdbuf);

	return {&meshGroup, meshGroup.version, vertexBuffer, indexBuffer, finalIndices.size()};
}

struct SDLMeshDestroyContext {
	Direct3DRMSDL3GPURenderer* renderer;
	Uint32 id;
};

void Direct3DRMSDL3GPURenderer::AddMeshDestroyCallback(Uint32 id, IDirect3DRMMesh* mesh)
{
	auto* ctx = new SDLMeshDestroyContext{this, id};
	mesh->AddDestroyCallback(
		[](IDirect3DRMObject*, void* arg) {
			auto* ctx = static_cast<SDLMeshDestroyContext*>(arg);
			auto& cache = ctx->renderer->m_meshs[ctx->id];
			SDL_ReleaseGPUBuffer(ctx->renderer->m_device, cache.vertexBuffer);
			SDL_ReleaseGPUBuffer(ctx->renderer->m_device, cache.indexBuffer);
			cache.meshGroup = nullptr;
			delete ctx;
		},
		ctx
	);
}

uint32_t Direct3DRMSDL3GPURenderer::GetMeshId(IDirect3DRMMesh* mesh, const MeshGroup* meshGroup)
{
	for (Uint32 i = 0; i < m_meshs.size(); ++i) {
		auto& cache = m_meshs[i];
		if (cache.meshGroup == meshGroup) {
			if (cache.version != meshGroup->version) {
				SDL_ReleaseGPUBuffer(m_device, cache.vertexBuffer);
				SDL_ReleaseGPUBuffer(m_device, cache.indexBuffer);
				cache = std::move(UploadMesh(*meshGroup));
			}
			return i;
		}
	}

	auto newCache = UploadMesh(*meshGroup);

	for (Uint32 i = 0; i < m_meshs.size(); ++i) {
		auto& cache = m_meshs[i];
		if (!cache.meshGroup) {
			cache = std::move(newCache);
			AddMeshDestroyCallback(i, mesh);
			return i;
		}
	}

	m_meshs.push_back(std::move(newCache));
	AddMeshDestroyCallback((Uint32) (m_meshs.size() - 1), mesh);
	return (Uint32) (m_meshs.size() - 1);
}

void PackNormalMatrix(const Matrix3x3& normalMatrix3x3, D3DRMMATRIX4D& packedNormalMatrix4x4)
{
	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 3; ++col) {
			packedNormalMatrix4x4[col][row] = normalMatrix3x3[row][col];
		}
		packedNormalMatrix4x4[3][row] = 0.0f;
	}
	for (int col = 0; col < 4; ++col) {
		packedNormalMatrix4x4[col][3] = 0.0f;
	}
}

SDL_GPUTransferBuffer* Direct3DRMSDL3GPURenderer::GetUploadBuffer(size_t size)
{
	WaitForPendingUpload();
	if (m_uploadBufferSize < size) {
		SDL_ReleaseGPUTransferBuffer(m_device, m_uploadBuffer);

		SDL_GPUTransferBufferCreateInfo transferCreateInfo = {};
		transferCreateInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
		transferCreateInfo.size = size;
		m_uploadBuffer = SDL_CreateGPUTransferBuffer(m_device, &transferCreateInfo);
		if (!m_uploadBuffer) {
			m_uploadBufferSize = 0;
			MORTAR_LogError(
				LOG_CATEGORY_MINIWIN,
				"SDL_CreateGPUTransferBuffer failed for updating upload buffer (%s)",
				MORTAR_GetError()
			);
		}
		m_uploadBufferSize = size;
	}

	return m_uploadBuffer;
}

void Direct3DRMSDL3GPURenderer::StartRenderPass(float r, float g, float b, bool clear)
{
	if (m_cmdbuf != nullptr) {
		SDL_CancelGPUCommandBuffer(m_cmdbuf);
	}
	m_cmdbuf = SDL_AcquireGPUCommandBuffer(m_device);
	if (!m_cmdbuf) {
		MORTAR_LogError(
			LOG_CATEGORY_MINIWIN,
			"SDL_AcquireGPUCommandBuffer in StartRenderPass failed (%s)",
			MORTAR_GetError()
		);
		return;
	}

	// Clear color and depth targets
	SDL_GPUColorTargetInfo colorTargetInfo = {};
	colorTargetInfo.texture = m_transferTexture;
	colorTargetInfo.clear_color = {r, g, b, 1.0f};
	colorTargetInfo.load_op = clear ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_DONT_CARE;

	SDL_GPUDepthStencilTargetInfo depthStencilTargetInfo = {};
	depthStencilTargetInfo.texture = m_depthTexture;
	depthStencilTargetInfo.clear_depth = 0.0f;
	depthStencilTargetInfo.load_op = clear ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_DONT_CARE;
	depthStencilTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

	m_renderPass = SDL_BeginGPURenderPass(m_cmdbuf, &colorTargetInfo, 1, &depthStencilTargetInfo);
}

void Direct3DRMSDL3GPURenderer::Clear(float r, float g, float b)
{
	StartRenderPass(r, g, b, true);
}

HRESULT Direct3DRMSDL3GPURenderer::BeginFrame()
{
	if (!m_renderPass) {
		StartRenderPass(0, 0, 0, false);
	}
	SDL_BindGPUGraphicsPipeline(m_renderPass, m_opaquePipeline);
	memcpy(&m_uniforms.projection, m_projection, sizeof(D3DRMMATRIX4D));

	return DD_OK;
}

void Direct3DRMSDL3GPURenderer::EnableTransparency()
{
	SDL_BindGPUGraphicsPipeline(m_renderPass, m_transparentPipeline);
}

void Direct3DRMSDL3GPURenderer::SubmitDraw(
	DWORD meshId,
	const D3DRMMATRIX4D& modelViewMatrix,
	const D3DRMMATRIX4D& worldMatrix,
	const D3DRMMATRIX4D& viewMatrix,
	const Matrix3x3& normalMatrix,
	const Appearance& appearance
)
{
	memcpy(&m_uniforms.worldViewMatrix, modelViewMatrix, sizeof(D3DRMMATRIX4D));
	PackNormalMatrix(normalMatrix, m_uniforms.normalMatrix);
	m_fragmentShadingData.color = to_SDL_Color(appearance.color);
	m_fragmentShadingData.shininess = appearance.shininess;
	bool useTexture = appearance.textureId != NO_TEXTURE_ID;
	m_fragmentShadingData.useTexture = appearance.textureId != NO_TEXTURE_ID;

	auto& mesh = m_meshs[meshId];

	// Render the graphics
	SDL_GPUTexture* texture = useTexture ? m_textures[appearance.textureId].gpuTexture : m_dummyTexture;
	SDL_GPUTextureSamplerBinding samplerBinding = {texture, m_sampler};
	SDL_BindGPUFragmentSamplers(m_renderPass, 0, &samplerBinding, 1);
	SDL_PushGPUVertexUniformData(m_cmdbuf, 0, &m_uniforms, sizeof(m_uniforms));
	SDL_PushGPUFragmentUniformData(m_cmdbuf, 0, &m_fragmentShadingData, sizeof(m_fragmentShadingData));
	SDL_GPUBufferBinding vertexBufferBinding = {mesh.vertexBuffer};
	SDL_BindGPUVertexBuffers(m_renderPass, 0, &vertexBufferBinding, 1);
	SDL_GPUBufferBinding indexBufferBinding = {mesh.indexBuffer};
	SDL_BindGPUIndexBuffer(m_renderPass, &indexBufferBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
	SDL_DrawGPUIndexedPrimitives(m_renderPass, mesh.indexCount, 1, 0, 0, 0);
}

HRESULT Direct3DRMSDL3GPURenderer::FinalizeFrame()
{
	return DD_OK;
}

void Direct3DRMSDL3GPURenderer::Resize(int width, int height, const ViewportTransform& viewportTransform)
{
	m_width = width;
	m_height = height;
	m_viewportTransform = viewportTransform;

	if (m_transferTexture) {
		SDL_ReleaseGPUTexture(m_device, m_transferTexture);
	}
	SDL_GPUTextureCreateInfo textureInfo = {};
	textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
	textureInfo.format = SDL_GetGPUSwapchainTextureFormat(m_device, to_SDL3_Window(DDWindow));
	textureInfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
	textureInfo.width = m_width;
	textureInfo.height = m_height;
	textureInfo.layer_count_or_depth = 1;
	textureInfo.num_levels = 1;
	textureInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
	m_transferTexture = SDL_CreateGPUTexture(m_device, &textureInfo);
	if (!m_transferTexture) {
		MORTAR_LogError(LOG_CATEGORY_MINIWIN, "SDL_CreateGPUTexture for backbuffer failed (%s)", MORTAR_GetError());
		return;
	}

	if (m_depthTexture) {
		SDL_ReleaseGPUTexture(m_device, m_depthTexture);
	}
	SDL_GPUTextureCreateInfo depthTexInfo = textureInfo;
	depthTexInfo.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
	depthTexInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
	m_depthTexture = SDL_CreateGPUTexture(m_device, &depthTexInfo);
	if (!m_depthTexture) {
		MORTAR_LogError(LOG_CATEGORY_MINIWIN, "SDL_CreateGPUTexture for depth buffer (%s)", MORTAR_GetError());
		return;
	}

	// Setup texture GPU-to-CPU transfer
	SDL_GPUTransferBufferCreateInfo downloadBufferInfo = {};
	downloadBufferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
	downloadBufferInfo.size =
		((m_width - (m_viewportTransform.offsetX * 2)) * (m_height - (m_viewportTransform.offsetY * 2))) * 4;
	m_downloadBuffer = SDL_CreateGPUTransferBuffer(m_device, &downloadBufferInfo);
	if (!m_downloadBuffer) {
		MORTAR_LogError(
			LOG_CATEGORY_MINIWIN,
			"SDL_CreateGPUTransferBuffer filed for download buffer (%s)",
			MORTAR_GetError()
		);
		return;
	}
}

void Direct3DRMSDL3GPURenderer::Flip()
{
	if (!m_cmdbuf) {
		return;
	}
	if (m_renderPass) {
		SDL_EndGPURenderPass(m_renderPass);
		m_renderPass = nullptr;
	}

	SDL_GPUTexture* swapchainTexture;
	if (!SDL_WaitAndAcquireGPUSwapchainTexture(
			m_cmdbuf,
			to_SDL3_Window(DDWindow),
			&swapchainTexture,
			nullptr,
			nullptr
		) ||
		!swapchainTexture) {
		MORTAR_Log("SDL_WaitAndAcquireGPUSwapchainTexture: %s", MORTAR_GetError());
		return;
	}

	SDL_GPUBlitInfo blit = {};
	blit.source.texture = m_transferTexture;
	blit.source.w = m_width;
	blit.source.h = m_height;
	blit.destination.texture = swapchainTexture;
	blit.destination.w = m_width;
	blit.destination.h = m_height;
	blit.load_op = SDL_GPU_LOADOP_DONT_CARE;
	blit.flip_mode = SDL_FLIP_NONE;
	blit.filter = SDL_GPU_FILTER_NEAREST;
	blit.cycle = false;
	SDL_BlitGPUTexture(m_cmdbuf, &blit);
	SDL_SubmitGPUCommandBuffer(m_cmdbuf);
	m_cmdbuf = nullptr;
}

void Direct3DRMSDL3GPURenderer::Draw2DImage(
	Uint32 textureId,
	const MORTAR_Rect& srcRect,
	const MORTAR_Rect& dstRect,
	FColor color
)
{
	if (!m_renderPass) {
		StartRenderPass(0, 0, 0, false);
	}
	SDL_BindGPUGraphicsPipeline(m_renderPass, m_uiPipeline);

	SDL_GPUTexture* tex;
	MORTAR_Rect expandedDstRect;
	if (textureId == NO_TEXTURE_ID) {
		expandedDstRect = dstRect;
		tex = m_dummyTexture;
	}
	else {
		SDL3TextureCache& cache = m_textures[textureId];
		tex = cache.gpuTexture;
		auto surface = static_cast<DirectDrawSurfaceImpl*>(cache.texture->m_surface);
		float scaleX = static_cast<float>(dstRect.w) / srcRect.w;
		float scaleY = static_cast<float>(dstRect.h) / srcRect.h;
		expandedDstRect = {
			static_cast<int>(std::round(dstRect.x - srcRect.x * scaleX)),
			static_cast<int>(std::round(dstRect.y - srcRect.y * scaleY)),
			static_cast<int>(std::round(static_cast<float>(surface->m_surface->w) * scaleX)),
			static_cast<int>(std::round(static_cast<float>(surface->m_surface->h) * scaleY)),
		};
	}

	Create2DTransformMatrix(
		expandedDstRect,
		m_viewportTransform.scale,
		m_viewportTransform.offsetX,
		m_viewportTransform.offsetY,
		m_uniforms.worldViewMatrix
	);

	CreateOrthographicProjection((float) m_width, (float) m_height, m_uniforms.projection);

	SceneLight fullBright = {{1, 1, 1, 1}, {0, 0, 0}, 0, {0, 0, 0}, 0};
	memcpy(&m_fragmentShadingData.lights, &fullBright, sizeof(SceneLight));
	m_fragmentShadingData.lightCount = 1;
	m_fragmentShadingData.color.r = static_cast<uint8_t>(color.r * 255);
	m_fragmentShadingData.color.g = static_cast<uint8_t>(color.g * 255);
	m_fragmentShadingData.color.b = static_cast<uint8_t>(color.b * 255);
	m_fragmentShadingData.color.a = static_cast<uint8_t>(color.a * 255);
	m_fragmentShadingData.shininess = 0.0f;
	m_fragmentShadingData.useTexture = 1;

	SDL_GPUTextureSamplerBinding samplerBinding = {tex, m_uiSampler};
	SDL_BindGPUFragmentSamplers(m_renderPass, 0, &samplerBinding, 1);
	SDL_PushGPUVertexUniformData(m_cmdbuf, 0, &m_uniforms, sizeof(m_uniforms));
	SDL_PushGPUFragmentUniformData(m_cmdbuf, 0, &m_fragmentShadingData, sizeof(m_fragmentShadingData));
	SDL_GPUBufferBinding vertexBufferBinding = {m_uiMeshCache.vertexBuffer};
	SDL_BindGPUVertexBuffers(m_renderPass, 0, &vertexBufferBinding, 1);
	SDL_GPUBufferBinding indexBufferBinding = {m_uiMeshCache.indexBuffer};
	SDL_BindGPUIndexBuffer(m_renderPass, &indexBufferBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

	SDL_Rect scissor;
	scissor.x = static_cast<int>(std::round(dstRect.x * m_viewportTransform.scale + m_viewportTransform.offsetX));
	scissor.y = static_cast<int>(std::round(dstRect.y * m_viewportTransform.scale + m_viewportTransform.offsetY));
	scissor.w = static_cast<int>(std::round(dstRect.w * m_viewportTransform.scale));
	scissor.h = static_cast<int>(std::round(dstRect.h * m_viewportTransform.scale));
	SDL_SetGPUScissor(m_renderPass, &scissor);

	SDL_DrawGPUIndexedPrimitives(m_renderPass, m_uiMeshCache.indexCount, 1, 0, 0, 0);

	SDL_Rect fullViewport = {0, 0, m_width, m_height};
	SDL_SetGPUScissor(m_renderPass, &fullViewport);
}

void Direct3DRMSDL3GPURenderer::SetDither(bool dither)
{
}

void Direct3DRMSDL3GPURenderer::Download(MORTAR_Surface* target)
{
	if (!m_cmdbuf) {
		StartRenderPass(0, 0, 0, false);
	}
	if (m_renderPass) {
		SDL_EndGPURenderPass(m_renderPass);
		m_renderPass = nullptr;
	}

	const int offsetX = static_cast<int>(m_viewportTransform.offsetX);
	const int offsetY = static_cast<int>(m_viewportTransform.offsetY);
	const int width = m_width - offsetX * 2;
	const int height = m_height - offsetY * 2;

	SDL_GPUTextureRegion region = {};
	region.texture = m_transferTexture;
	region.x = offsetX;
	region.y = offsetY;
	region.w = width;
	region.h = height;
	region.d = 1;

	SDL_GPUTextureTransferInfo transferInfo = {};
	transferInfo.transfer_buffer = m_downloadBuffer;

	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(m_cmdbuf);
	SDL_DownloadFromGPUTexture(copyPass, &region, &transferInfo);
	SDL_EndGPUCopyPass(copyPass);

	WaitForPendingUpload();

	SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(m_cmdbuf);
	m_cmdbuf = nullptr;

	if (!fence || !SDL_WaitForGPUFences(m_device, true, &fence, 1)) {
		SDL_ReleaseGPUFence(m_device, fence);
		return;
	}
	SDL_ReleaseGPUFence(m_device, fence);

	void* downloadedData = SDL_MapGPUTransferBuffer(m_device, m_downloadBuffer, false);
	if (!downloadedData) {
		return;
	}

	SDL_Surface* renderedImage =
		SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_XRGB8888, downloadedData, width * 4);

	SDL_BlitSurfaceScaled(
		renderedImage,
		nullptr,
		(SDL_Surface*) MORTAR_EXT_GetSurfaceProperty(target, MORTAR_SURFACE_PROPERTY_SDL3SURFACE, nullptr),
		nullptr,
		SDL_SCALEMODE_NEAREST
	);
	SDL_DestroySurface(renderedImage);
	SDL_UnmapGPUTransferBuffer(m_device, m_downloadBuffer);
}

void Direct3DRMSDL3GPU_EnumDevice(LPD3DENUMDEVICESCALLBACK cb, void* ctx)
{
	if (MORTAR_GetBackend() != MORTAR_BACKEND_SDL3) {
		return;
	}
#ifdef __APPLE__
	SDL_GPUDevice* device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, false, nullptr);
	if (!device) {
		return;
	}
	SDL_DestroyGPUDevice(device);
#else
	Direct3DRMRenderer* device = Direct3DRMSDL3GPURenderer::Create(640, 480);
	if (!device) {
		return;
	}
	device->Release();
#endif

	D3DDEVICEDESC halDesc = {};
	halDesc.dcmColorModel = D3DCOLOR_RGB;
	halDesc.dwFlags = D3DDD_DEVICEZBUFFERBITDEPTH;
	halDesc.dwDeviceZBufferBitDepth = DDBD_32;
	halDesc.dwDeviceRenderBitDepth = DDBD_32;
	halDesc.dpcTriCaps.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE;
	halDesc.dpcTriCaps.dwShadeCaps = D3DPSHADECAPS_ALPHAFLATBLEND;
	halDesc.dpcTriCaps.dwTextureFilterCaps = D3DPTFILTERCAPS_LINEAR;

	D3DDEVICEDESC helDesc = {};

	EnumDevice(cb, ctx, "SDL3 GPU HAL", &halDesc, &helDesc, SDL3_GPU_GUID);
}

Direct3DRMRenderer* Create_Direct3DRMSDL3GPURenderer(DWORD width, DWORD height)
{
	return Direct3DRMSDL3GPURenderer::Create(width, height);
}
