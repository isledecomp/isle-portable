#include "ShaderIndex.h"
#include "d3drmrenderer.h"
#include "d3drmrenderer_sdl3gpu.h"
#include "ddraw_impl.h"
#include "mathutils.h"
#include "miniwin.h"

#include <SDL3/SDL.h>
#include <cstddef>

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

SDL_GPUTexture* CreateTextureFromSurface(
	SDL_GPUDevice* device,
	SDL_GPUTransferBuffer* transferBuffer,
	SDL_Surface* surface
)
{
	ScopedSurface surf{SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888)};
	if (!surf.ptr) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_ConvertSurface (%s)", SDL_GetError());
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
	ScopedTexture texture{device, SDL_CreateGPUTexture(device, &textureInfo)};
	if (!texture.ptr) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_CreateGPUTexture (%s)", SDL_GetError());
		return nullptr;
	}

	void* transferData = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
	if (!transferData) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_MapGPUTransferBuffer (%s)", SDL_GetError());
		return nullptr;
	}

	memcpy(transferData, surf.ptr->pixels, dataSize);
	SDL_UnmapGPUTransferBuffer(device, transferBuffer);

	SDL_GPUTextureTransferInfo transferRegionInfo = {};
	transferRegionInfo.transfer_buffer = transferBuffer;
	SDL_GPUTextureRegion textureRegion = {};
	textureRegion.texture = texture.ptr;
	textureRegion.w = surf.ptr->w;
	textureRegion.h = surf.ptr->h;
	textureRegion.d = 1;
	SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(device);
	if (!cmdbuf) {
		SDL_LogError(
			LOG_CATEGORY_MINIWIN,
			"SDL_AcquireGPUCommandBuffer in CreateTextureFromSurface failed (%s)",
			SDL_GetError()
		);
		return nullptr;
	}
	SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(cmdbuf);
	SDL_UploadToGPUTexture(pass, &transferRegionInfo, &textureRegion, false);

	SDL_EndGPUCopyPass(pass);
	if (!SDL_SubmitGPUCommandBuffer(cmdbuf)) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_SubmitGPUCommandBuffer (%s)", SDL_GetError());
		return nullptr;
	}

	auto texptr = texture.ptr;

	// Release texture ownership so caller can manage it safely
	texture.release();

	return texptr;
}

static SDL_GPUGraphicsPipeline* InitializeGraphicsPipeline(SDL_GPUDevice* device, bool depthWrite)
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
	colorTargets.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	if (depthWrite) {
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
	pipelineCreateInfo.depth_stencil_state.enable_depth_test = true;
	pipelineCreateInfo.depth_stencil_state.enable_depth_write = depthWrite;
	pipelineCreateInfo.depth_stencil_state.enable_stencil_test = false;
	pipelineCreateInfo.target_info.color_target_descriptions = &colorTargets;
	pipelineCreateInfo.target_info.num_color_targets = 1;
	pipelineCreateInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
	pipelineCreateInfo.target_info.has_depth_stencil_target = true;

	SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineCreateInfo);

	return pipeline;
}

Direct3DRMRenderer* Direct3DRMSDL3GPURenderer::Create(DWORD width, DWORD height)
{
	ScopedDevice device{SDL_CreateGPUDevice(
		SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL,
		true,
		nullptr
	)};
	if (!device.ptr) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_CreateGPUDevice failed (%s)", SDL_GetError());
		return nullptr;
	}

	ScopedPipeline opaquePipeline{device.ptr, InitializeGraphicsPipeline(device.ptr, true)};
	if (!opaquePipeline.ptr) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "InitializeGraphicsPipeline for opaquePipeline");
		return nullptr;
	}

	ScopedPipeline transparentPipeline{device.ptr, InitializeGraphicsPipeline(device.ptr, false)};
	if (!transparentPipeline.ptr) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "InitializeGraphicsPipeline for transparentPipeline");
		return nullptr;
	}

	SDL_GPUTextureCreateInfo textureInfo = {};
	textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
	textureInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	textureInfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
	textureInfo.width = width;
	textureInfo.height = height;
	textureInfo.layer_count_or_depth = 1;
	textureInfo.num_levels = 1;
	textureInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
	ScopedTexture transferTexture{device.ptr, SDL_CreateGPUTexture(device.ptr, &textureInfo)};
	if (!transferTexture.ptr) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_CreateGPUTexture for backbuffer failed (%s)", SDL_GetError());
		return nullptr;
	}

	SDL_GPUTextureCreateInfo depthTexInfo = textureInfo;
	depthTexInfo.format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
	depthTexInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
	ScopedTexture depthTexture{device.ptr, SDL_CreateGPUTexture(device.ptr, &depthTexInfo)};
	if (!depthTexture.ptr) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_CreateGPUTexture for depth buffer (%s)", SDL_GetError());
		return nullptr;
	}

	// Setup texture GPU-to-CPU transfer
	SDL_GPUTransferBufferCreateInfo downloadBufferInfo = {};
	downloadBufferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
	downloadBufferInfo.size = width * height * 4;
	ScopedTransferBuffer downloadBuffer{device.ptr, SDL_CreateGPUTransferBuffer(device.ptr, &downloadBufferInfo)};
	if (!downloadBuffer.ptr) {
		SDL_LogError(
			LOG_CATEGORY_MINIWIN,
			"SDL_CreateGPUTransferBuffer filed for download buffer (%s)",
			SDL_GetError()
		);
		return nullptr;
	}

	// Setup texture CPU-to-GPU transfer
	SDL_GPUTransferBufferCreateInfo uploadBufferInfo = {};
	uploadBufferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	uploadBufferInfo.size = 256 * 256 * 4; // Largest known game texture
	ScopedTransferBuffer uploadBuffer{device.ptr, SDL_CreateGPUTransferBuffer(device.ptr, &uploadBufferInfo)};
	if (!uploadBuffer.ptr) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_CreateGPUTransferBuffer filed for upload buffer (%s)", SDL_GetError());
		return nullptr;
	}

	SDL_Surface* dummySurface = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_ABGR8888);
	if (!dummySurface) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "Failed to create surface: %s", SDL_GetError());
		return nullptr;
	}
	if (!SDL_LockSurface(dummySurface)) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "Failed to lock surface: %s", SDL_GetError());
		SDL_DestroySurface(dummySurface);
		return nullptr;
	}
	((Uint32*) dummySurface->pixels)[0] = 0xFFFFFFFF;

	ScopedTexture dummyTexture{device.ptr, CreateTextureFromSurface(device.ptr, uploadBuffer.ptr, dummySurface)};
	SDL_DestroySurface(dummySurface);
	if (!dummyTexture.ptr) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "Failed to create texture from surface");
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
		SDL_LogError(LOG_CATEGORY_MINIWIN, "Failed to create sampler: %s", SDL_GetError());
		return nullptr;
	}

	auto renderer = new Direct3DRMSDL3GPURenderer(
		width,
		height,
		device.ptr,
		opaquePipeline.ptr,
		transparentPipeline.ptr,
		transferTexture.ptr,
		depthTexture.ptr,
		dummyTexture.ptr,
		sampler.ptr,
		uploadBuffer.ptr,
		downloadBuffer.ptr
	);

	// Release resources so they don't get cleaned up
	device.release();
	opaquePipeline.release();
	transparentPipeline.release();
	transferTexture.release();
	depthTexture.release();
	dummyTexture.release();
	sampler.release();
	uploadBuffer.release();
	downloadBuffer.release();

	return renderer;
}

Direct3DRMSDL3GPURenderer::Direct3DRMSDL3GPURenderer(
	DWORD width,
	DWORD height,
	SDL_GPUDevice* device,
	SDL_GPUGraphicsPipeline* opaquePipeline,
	SDL_GPUGraphicsPipeline* transparentPipeline,
	SDL_GPUTexture* transferTexture,
	SDL_GPUTexture* depthTexture,
	SDL_GPUTexture* dummyTexture,
	SDL_GPUSampler* sampler,
	SDL_GPUTransferBuffer* uploadBuffer,
	SDL_GPUTransferBuffer* downloadBuffer
)
	: m_width(width), m_height(height), m_device(device), m_opaquePipeline(opaquePipeline),
	  m_transparentPipeline(transparentPipeline), m_transferTexture(transferTexture), m_depthTexture(depthTexture),
	  m_dummyTexture(dummyTexture), m_sampler(sampler), m_uploadBuffer(uploadBuffer), m_downloadBuffer(downloadBuffer)
{
}

Direct3DRMSDL3GPURenderer::~Direct3DRMSDL3GPURenderer()
{
	SDL_ReleaseGPUTransferBuffer(m_device, m_downloadBuffer);
	if (m_uploadBuffer) {
		SDL_ReleaseGPUTransferBuffer(m_device, m_uploadBuffer);
	}
	SDL_ReleaseGPUSampler(m_device, m_sampler);
	SDL_ReleaseGPUTexture(m_device, m_dummyTexture);
	SDL_ReleaseGPUTexture(m_device, m_depthTexture);
	SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer);
	SDL_ReleaseGPUTexture(m_device, m_transferTexture);
	SDL_ReleaseGPUGraphicsPipeline(m_device, m_opaquePipeline);
	SDL_ReleaseGPUGraphicsPipeline(m_device, m_transparentPipeline);
	SDL_DestroyGPUDevice(m_device);
}

void Direct3DRMSDL3GPURenderer::PushLights(const SceneLight* vertices, size_t count)
{
	if (count > 3) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "Unsupported number of lights (%d)", static_cast<int>(count));
		count = 3;
	}
	memcpy(&m_fragmentShadingData.lights, vertices, sizeof(SceneLight) * count);
	m_fragmentShadingData.lightCount = count;
}

void Direct3DRMSDL3GPURenderer::SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back)
{
	m_front = front;
	m_back = back;
	memcpy(&m_uniforms.projection, projection, sizeof(D3DRMMATRIX4D));
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

Uint32 Direct3DRMSDL3GPURenderer::GetTextureId(IDirect3DRMTexture* iTexture)
{
	auto texture = dynamic_cast<Direct3DRMTextureImpl*>(iTexture);
	auto surface = dynamic_cast<DirectDrawSurfaceImpl*>(texture->m_surface);
	SDL_Surface* surf = surface->m_surface;

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (tex.texture == texture) {
			if (tex.version != texture->m_version) {
				SDL_ReleaseGPUTexture(m_device, tex.gpuTexture);
				tex.gpuTexture = CreateTextureFromSurface(m_device, GetUploadBuffer(surf->w * surf->h * 4), surf);
				if (!tex.gpuTexture) {
					return NO_TEXTURE_ID;
				}
				tex.version = texture->m_version;
			}
			return i;
		}
	}

	SDL_GPUTexture* newTex = CreateTextureFromSurface(m_device, GetUploadBuffer(surf->w * surf->h * 4), surf);
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

DWORD Direct3DRMSDL3GPURenderer::GetWidth()
{
	return m_width;
}

DWORD Direct3DRMSDL3GPURenderer::GetHeight()
{
	return m_height;
}

void Direct3DRMSDL3GPURenderer::GetDesc(D3DDEVICEDESC* halDesc, D3DDEVICEDESC* helDesc)
{
	halDesc->dcmColorModel = D3DCOLORMODEL::RGB;
	halDesc->dwFlags = D3DDD_DEVICEZBUFFERBITDEPTH;
	halDesc->dwDeviceZBufferBitDepth = DDBD_16; // Todo add support for other depths
	halDesc->dwDeviceRenderBitDepth = DDBD_32;
	halDesc->dpcTriCaps.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE;
	halDesc->dpcTriCaps.dwShadeCaps = D3DPSHADECAPS_ALPHAFLATBLEND;
	halDesc->dpcTriCaps.dwTextureFilterCaps = D3DPTFILTERCAPS_LINEAR;

	memset(helDesc, 0, sizeof(D3DDEVICEDESC));
}

const char* Direct3DRMSDL3GPURenderer::GetName()
{
	return "SDL3 GPU HAL";
}

HRESULT Direct3DRMSDL3GPURenderer::BeginFrame(const D3DRMMATRIX4D& viewMatrix)
{
	if (!DDBackBuffer) {
		return DDERR_GENERIC;
	}

	memcpy(&m_viewMatrix, viewMatrix, sizeof(D3DRMMATRIX4D));

	// Clear color and depth targets
	SDL_GPUColorTargetInfo colorTargetInfo = {};
	colorTargetInfo.texture = m_transferTexture;
	colorTargetInfo.clear_color = {0, 0, 0, 0};
	colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;

	SDL_GPUDepthStencilTargetInfo depthStencilTargetInfo = {};
	depthStencilTargetInfo.texture = m_depthTexture;
	depthStencilTargetInfo.clear_depth = 0.0f;
	depthStencilTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;

	SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(m_device);
	if (!cmdbuf) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_AcquireGPUCommandBuffer in BeginFrame failed (%s)", SDL_GetError());
		return DDERR_GENERIC;
	}

	SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, &depthStencilTargetInfo);
	SDL_EndGPURenderPass(renderPass);

	if (!SDL_SubmitGPUCommandBuffer(cmdbuf)) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_SubmitGPUCommandBuffer failed (%s)", SDL_GetError());
		return DDERR_GENERIC;
	}

	return DD_OK;
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
	if (m_uploadBufferSize < size) {
		SDL_ReleaseGPUTransferBuffer(m_device, m_uploadBuffer);

		SDL_GPUTransferBufferCreateInfo transferCreateInfo = {};
		transferCreateInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
		transferCreateInfo.size = size;
		m_uploadBuffer = SDL_CreateGPUTransferBuffer(m_device, &transferCreateInfo);
		if (!m_uploadBuffer) {
			m_uploadBufferSize = 0;
			SDL_LogError(
				LOG_CATEGORY_MINIWIN,
				"SDL_CreateGPUTransferBuffer failed for updating upload buffer (%s)",
				SDL_GetError()
			);
		}
		m_uploadBufferSize = size;
	}

	return m_uploadBuffer;
}

void Direct3DRMSDL3GPURenderer::SubmitDraw(
	const D3DRMVERTEX* vertices,
	const size_t count,
	const D3DRMMATRIX4D& worldMatrix,
	const Matrix3x3& normalMatrix,
	const Appearance& appearance
)
{
	D3DRMMATRIX4D worldViewMatrix;
	MultiplyMatrix(worldViewMatrix, worldMatrix, m_viewMatrix);
	memcpy(&m_uniforms.worldViewMatrix, worldViewMatrix, sizeof(D3DRMMATRIX4D));
	PackNormalMatrix(normalMatrix, m_uniforms.normalMatrix);
	m_fragmentShadingData.color = appearance.color;
	m_fragmentShadingData.shininess = appearance.shininess;

	if (count > m_vertexBufferCount) {
		if (m_vertexBuffer) {
			SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer);
		}
		SDL_GPUBufferCreateInfo bufferCreateInfo = {};
		bufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
		bufferCreateInfo.size = static_cast<Uint32>(sizeof(D3DRMVERTEX) * count);
		m_vertexBuffer = SDL_CreateGPUBuffer(m_device, &bufferCreateInfo);
		if (!m_vertexBuffer) {
			SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_CreateGPUBuffer failed (%s)", SDL_GetError());
		}
		m_vertexBufferCount = count;
	}

	m_vertexCount = count;

	SDL_GPUTransferBuffer* uploadBuffer = GetUploadBuffer(sizeof(D3DRMVERTEX) * m_vertexCount);
	if (!uploadBuffer) {
		return;
	}
	D3DRMVERTEX* transferData = (D3DRMVERTEX*) SDL_MapGPUTransferBuffer(m_device, uploadBuffer, false);
	if (!transferData) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_MapGPUTransferBuffer returned NULL buffer (%s)", SDL_GetError());
		return;
	}

	memcpy(transferData, vertices, m_vertexCount * sizeof(D3DRMVERTEX));
	SDL_UnmapGPUTransferBuffer(m_device, uploadBuffer);

	// Upload the transfer data to the vertex buffer
	SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(m_device);
	if (!cmdbuf) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_AcquireGPUCommandBuffer in SubmitDraw failed (%s)", SDL_GetError());
		return;
	}

	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdbuf);
	SDL_GPUTransferBufferLocation transferLocation = {};
	transferLocation.transfer_buffer = uploadBuffer;

	SDL_GPUBufferRegion bufferRegion = {};
	bufferRegion.buffer = m_vertexBuffer;
	bufferRegion.size = static_cast<Uint32>(sizeof(D3DRMVERTEX) * m_vertexCount);

	SDL_UploadToGPUBuffer(copyPass, &transferLocation, &bufferRegion, false);
	SDL_EndGPUCopyPass(copyPass);

	// Render the graphics
	SDL_GPUColorTargetInfo colorTargetInfo = {};
	colorTargetInfo.texture = m_transferTexture;
	colorTargetInfo.load_op = SDL_GPU_LOADOP_LOAD;

	SDL_GPUDepthStencilTargetInfo depthStencilTargetInfo = {};
	depthStencilTargetInfo.texture = m_depthTexture;
	depthStencilTargetInfo.load_op = SDL_GPU_LOADOP_LOAD;
	depthStencilTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

	SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, &depthStencilTargetInfo);
	SDL_BindGPUGraphicsPipeline(renderPass, appearance.color.a == 255 ? m_opaquePipeline : m_transparentPipeline);

	m_fragmentShadingData.useTexture = appearance.textureId != NO_TEXTURE_ID;
	SDL_GPUTextureSamplerBinding samplerBinding = {};
	samplerBinding.texture =
		m_fragmentShadingData.useTexture ? m_textures[appearance.textureId].gpuTexture : m_dummyTexture;
	samplerBinding.sampler = m_sampler;
	SDL_BindGPUFragmentSamplers(renderPass, 0, &samplerBinding, 1);

	SDL_PushGPUVertexUniformData(cmdbuf, 0, &m_uniforms, sizeof(m_uniforms));
	SDL_PushGPUFragmentUniformData(cmdbuf, 0, &m_fragmentShadingData, sizeof(m_fragmentShadingData));

	if (m_vertexCount) {
		SDL_GPUBufferBinding vertexBufferBinding = {};
		vertexBufferBinding.buffer = m_vertexBuffer;
		vertexBufferBinding.offset = 0;
		SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBufferBinding, 1);
		SDL_DrawGPUPrimitives(renderPass, m_vertexCount, 1, 0, 0);
	}

	SDL_EndGPURenderPass(renderPass);

	SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmdbuf);
	if (!fence || !SDL_WaitForGPUFences(m_device, true, &fence, 1)) {
		if (fence) {
			SDL_ReleaseGPUFence(m_device, fence);
		}
		return;
	}
	SDL_ReleaseGPUFence(m_device, fence);
}

HRESULT Direct3DRMSDL3GPURenderer::FinalizeFrame()
{
	// Download rendered image
	SDL_GPUTextureRegion region = {};
	region.texture = m_transferTexture;
	region.w = m_width;
	region.h = m_height;
	region.d = 1;
	SDL_GPUTextureTransferInfo transferInfo = {};
	transferInfo.transfer_buffer = m_downloadBuffer;

	SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(m_device);
	if (!cmdbuf) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_AcquireGPUCommandBuffer in FinalizeFrame failed (%s)", SDL_GetError());
		return DDERR_GENERIC;
	}

	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdbuf);
	SDL_DownloadFromGPUTexture(copyPass, &region, &transferInfo);
	SDL_EndGPUCopyPass(copyPass);
	SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmdbuf);
	if (!fence || !SDL_WaitForGPUFences(m_device, true, &fence, 1)) {
		if (fence) {
			SDL_ReleaseGPUFence(m_device, fence);
		}
		return DDERR_GENERIC;
	}
	SDL_ReleaseGPUFence(m_device, fence);
	void* downloadedData = SDL_MapGPUTransferBuffer(m_device, m_downloadBuffer, false);
	if (!downloadedData) {
		return DDERR_GENERIC;
	}

	SDL_Surface* renderedImage =
		SDL_CreateSurfaceFrom(m_width, m_height, SDL_PIXELFORMAT_ABGR8888, downloadedData, m_width * 4);
	SDL_BlitSurface(renderedImage, nullptr, DDBackBuffer, nullptr);
	SDL_DestroySurface(renderedImage);
	SDL_UnmapGPUTransferBuffer(m_device, m_downloadBuffer);

	return DD_OK;
}

HRESULT Direct3DRMSDL3GPURenderer::GetDescription(char* buffer, int bufferSize)
{
	SDL_strlcpy(buffer, "Miniwin SDL3_gpu renderer", bufferSize);
	return DD_OK;
}
