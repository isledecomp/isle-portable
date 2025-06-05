#include "ShaderIndex.h"
#include "d3drmrenderer.h"
#include "d3drmrenderer_sdl3gpu.h"
#include "ddraw_impl.h"
#include "mathutils.h"
#include "miniwin.h"

#include <SDL3/SDL.h>
#include <cstddef>

static SDL_GPUGraphicsPipeline* InitializeGraphicsPipeline(SDL_GPUDevice* device, bool depthWrite)
{
	const SDL_GPUShaderCreateInfo* vertexCreateInfo =
		GetVertexShaderCode(VertexShaderId::PositionColor, SDL_GetGPUShaderFormats(device));
	if (!vertexCreateInfo) {
		return nullptr;
	}
	SDL_GPUShader* vertexShader = SDL_CreateGPUShader(device, vertexCreateInfo);
	if (!vertexShader) {
		return nullptr;
	}

	const SDL_GPUShaderCreateInfo* fragmentCreateInfo =
		GetFragmentShaderCode(FragmentShaderId::SolidColor, SDL_GetGPUShaderFormats(device));
	if (!fragmentCreateInfo) {
		return nullptr;
	}
	SDL_GPUShader* fragmentShader = SDL_CreateGPUShader(device, fragmentCreateInfo);
	if (!fragmentShader) {
		return nullptr;
	}

	SDL_GPUVertexBufferDescription vertexBufferDescs[1] = {};
	vertexBufferDescs[0].slot = 0;
	vertexBufferDescs[0].pitch = sizeof(GeometryVertex);
	vertexBufferDescs[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
	vertexBufferDescs[0].instance_step_rate = 0;

	SDL_GPUVertexAttribute vertexAttrs[3] = {};
	vertexAttrs[0].location = 0;
	vertexAttrs[0].buffer_slot = 0;
	vertexAttrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	vertexAttrs[0].offset = offsetof(GeometryVertex, position);

	vertexAttrs[1].location = 1;
	vertexAttrs[1].buffer_slot = 0;
	vertexAttrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	vertexAttrs[1].offset = offsetof(GeometryVertex, normals);

	vertexAttrs[2].location = 2;
	vertexAttrs[2].buffer_slot = 0;
	vertexAttrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
	vertexAttrs[2].offset = offsetof(GeometryVertex, texCoord);

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
	pipelineCreateInfo.rasterizer_state = rasterizerState;
	pipelineCreateInfo.vertex_shader = vertexShader;
	pipelineCreateInfo.fragment_shader = fragmentShader;
	pipelineCreateInfo.vertex_input_state = vertexInputState;
	pipelineCreateInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	pipelineCreateInfo.target_info.color_target_descriptions = &colorTargets;
	pipelineCreateInfo.target_info.num_color_targets = 1;
	pipelineCreateInfo.target_info.has_depth_stencil_target = true;
	pipelineCreateInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
	pipelineCreateInfo.depth_stencil_state.enable_depth_test = true;
	pipelineCreateInfo.depth_stencil_state.enable_depth_write = depthWrite;
	pipelineCreateInfo.depth_stencil_state.enable_stencil_test = false;
	pipelineCreateInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_GREATER;
	pipelineCreateInfo.depth_stencil_state.write_mask = 0xff;

	SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineCreateInfo);
	// Clean up shader resources
	SDL_ReleaseGPUShader(device, vertexShader);
	SDL_ReleaseGPUShader(device, fragmentShader);

	return pipeline;
}

Direct3DRMRenderer* Direct3DRMSDL3GPURenderer::Create(DWORD width, DWORD height)
{
	SDL_GPUDevice* device = SDL_CreateGPUDevice(
		SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL,
		true,
		NULL
	);
	if (device == NULL) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_CreateGPUDevice failed (%s)", SDL_GetError());
		return nullptr;
	}
	SDL_GPUGraphicsPipeline* opaquePipeline = InitializeGraphicsPipeline(device, true);
	if (!opaquePipeline) {
		return nullptr;
	}
	SDL_GPUGraphicsPipeline* transparentPipeline = InitializeGraphicsPipeline(device, false);
	if (!transparentPipeline) {
		SDL_ReleaseGPUGraphicsPipeline(device, opaquePipeline);
		return nullptr;
	}

	SDL_GPUTextureCreateInfo textureInfo = {};
	textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
	textureInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	textureInfo.width = width;
	textureInfo.height = height;
	textureInfo.layer_count_or_depth = 1;
	textureInfo.num_levels = 1;
	textureInfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
	SDL_GPUTexture* transferTexture = SDL_CreateGPUTexture(device, &textureInfo);
	if (!transferTexture) {
		SDL_ReleaseGPUGraphicsPipeline(device, opaquePipeline);
		SDL_ReleaseGPUGraphicsPipeline(device, transparentPipeline);
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_CreateGPUTexture for backbuffer failed (%s)", SDL_GetError());
		return nullptr;
	}

	SDL_GPUTextureCreateInfo depthTextureInfo = {};
	depthTextureInfo.type = SDL_GPU_TEXTURETYPE_2D;
	depthTextureInfo.format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
	depthTextureInfo.width = width;
	depthTextureInfo.height = height;
	depthTextureInfo.layer_count_or_depth = 1;
	depthTextureInfo.num_levels = 1;
	depthTextureInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
	SDL_GPUTexture* depthTexture = SDL_CreateGPUTexture(device, &depthTextureInfo);
	if (!depthTexture) {
		SDL_ReleaseGPUGraphicsPipeline(device, opaquePipeline);
		SDL_ReleaseGPUGraphicsPipeline(device, transparentPipeline);
		SDL_ReleaseGPUTexture(device, transferTexture);
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_CreateGPUTexture for depth buffer (%s)", SDL_GetError());
		return nullptr;
	}

	// Setup texture GPU-to-CPU transfer
	SDL_GPUTransferBufferCreateInfo downloadTransferInfo = {};
	downloadTransferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
	downloadTransferInfo.size = static_cast<Uint32>(width * height * 4);
	SDL_GPUTransferBuffer* downloadTransferBuffer = SDL_CreateGPUTransferBuffer(device, &downloadTransferInfo);
	if (!downloadTransferBuffer) {
		SDL_ReleaseGPUGraphicsPipeline(device, opaquePipeline);
		SDL_ReleaseGPUGraphicsPipeline(device, transparentPipeline);
		SDL_ReleaseGPUTexture(device, depthTexture);
		SDL_ReleaseGPUTexture(device, transferTexture);
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_CreateGPUTransferBuffer failed (%s)", SDL_GetError());
		return nullptr;
	}

	return new Direct3DRMSDL3GPURenderer(
		width,
		height,
		device,
		opaquePipeline,
		transparentPipeline,
		transferTexture,
		depthTexture,
		downloadTransferBuffer
	);
}

Direct3DRMSDL3GPURenderer::Direct3DRMSDL3GPURenderer(
	DWORD width,
	DWORD height,
	SDL_GPUDevice* device,
	SDL_GPUGraphicsPipeline* opaquePipeline,
	SDL_GPUGraphicsPipeline* transparentPipeline,
	SDL_GPUTexture* transferTexture,
	SDL_GPUTexture* depthTexture,
	SDL_GPUTransferBuffer* downloadTransferBuffer
)
	: m_width(width), m_height(height), m_device(device), m_opaquePipeline(opaquePipeline),
	  m_transparentPipeline(transparentPipeline), m_transferTexture(transferTexture), m_depthTexture(depthTexture),
	  m_downloadTransferBuffer(downloadTransferBuffer)
{
}

Direct3DRMSDL3GPURenderer::~Direct3DRMSDL3GPURenderer()
{
	SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer);
	SDL_ReleaseGPUTransferBuffer(m_device, m_downloadTransferBuffer);
	SDL_ReleaseGPUTexture(m_device, m_depthTexture);
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

Uint32 Direct3DRMSDL3GPURenderer::GetTextureId(IDirect3DRMTexture* texture)
{
	return NO_TEXTURE_ID;
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
	depthStencilTargetInfo.clear_depth = 0.f;
	depthStencilTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;

	SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(m_device);
	if (!cmdbuf) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_AcquireGPUCommandBuffer failed (%s)", SDL_GetError());
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

void Direct3DRMSDL3GPURenderer::SubmitDraw(
	const GeometryVertex* vertices,
	const size_t count,
	const D3DRMMATRIX4D& worldMatrix,
	const Matrix3x3& normalMatrix,
	const Appearance& appearance
)
{
	D3DRMMATRIX4D worldViewMatrix;
	MultiplyMatrix(worldViewMatrix, worldMatrix, m_viewMatrix);
	memcpy(&m_uniforms.worldViewMatrix, worldViewMatrix, sizeof(D3DRMMATRIX4D));
	m_fragmentShadingData.color = appearance.color;
	m_fragmentShadingData.shininess = appearance.shininess;

	if (count > m_vertexBufferCount) {
		if (m_vertexBuffer) {
			SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer);
		}
		SDL_GPUBufferCreateInfo bufferCreateInfo = {};
		bufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
		bufferCreateInfo.size = static_cast<Uint32>(sizeof(GeometryVertex) * count);
		m_vertexBuffer = SDL_CreateGPUBuffer(m_device, &bufferCreateInfo);
		if (!m_vertexBuffer) {
			SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_CreateGPUBuffer returned NULL buffer (%s)", SDL_GetError());
		}
		m_vertexBufferCount = count;
	}

	m_vertexCount = count;
	if (!count) {
		return;
	}

	SDL_GPUTransferBufferCreateInfo transferCreateInfo = {};
	transferCreateInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	transferCreateInfo.size = static_cast<Uint32>(sizeof(GeometryVertex) * m_vertexCount);
	SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(m_device, &transferCreateInfo);
	if (!transferBuffer) {
		SDL_LogError(
			LOG_CATEGORY_MINIWIN,
			"SDL_CreateGPUTransferBuffer returned NULL transfer buffer (%s)",
			SDL_GetError()
		);
	}

	GeometryVertex* transferData = (GeometryVertex*) SDL_MapGPUTransferBuffer(m_device, transferBuffer, false);
	if (!transferData) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_MapGPUTransferBuffer returned NULL buffer (%s)", SDL_GetError());
	}

	memcpy(transferData, vertices, m_vertexCount * sizeof(GeometryVertex));

	SDL_UnmapGPUTransferBuffer(m_device, transferBuffer);

	// Upload the transfer data to the vertex buffer
	SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(m_device);
	if (!cmdbuf) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_AcquireGPUCommandBuffer failed (%s)", SDL_GetError());
		return;
	}

	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdbuf);
	SDL_GPUTransferBufferLocation transferLocation = {};
	transferLocation.transfer_buffer = transferBuffer;
	transferLocation.offset = 0;

	SDL_GPUBufferRegion bufferRegion = {};
	bufferRegion.buffer = m_vertexBuffer;
	bufferRegion.offset = 0;
	bufferRegion.size = static_cast<Uint32>(sizeof(GeometryVertex) * m_vertexCount);

	SDL_UploadToGPUBuffer(copyPass, &transferLocation, &bufferRegion, false);

	SDL_EndGPUCopyPass(copyPass);
	SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);

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

	if (!SDL_SubmitGPUCommandBuffer(cmdbuf)) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_SubmitGPUCommandBuffer failes (%s)", SDL_GetError());
	}
}

HRESULT Direct3DRMSDL3GPURenderer::FinalizeFrame()
{
	SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(m_device);
	if (cmdbuf == NULL) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_AcquireGPUCommandBuffer failed (%s)", SDL_GetError());
		return DDERR_GENERIC;
	}

	// Download rendered image
	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdbuf);
	SDL_GPUTextureRegion region = {};
	region.texture = m_transferTexture;
	region.w = m_width;
	region.h = m_height;
	region.d = 1;
	SDL_GPUTextureTransferInfo transferInfo = {};
	transferInfo.transfer_buffer = m_downloadTransferBuffer;
	transferInfo.offset = 0;
	SDL_DownloadFromGPUTexture(copyPass, &region, &transferInfo);
	SDL_EndGPUCopyPass(copyPass);
	SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmdbuf);
	if (!SDL_WaitForGPUFences(m_device, true, &fence, 1)) {
		return DDERR_GENERIC;
	}
	SDL_ReleaseGPUFence(m_device, fence);
	void* downloadedData = SDL_MapGPUTransferBuffer(m_device, m_downloadTransferBuffer, false);
	if (!downloadedData) {
		return DDERR_GENERIC;
	}

	SDL_DestroySurface(m_renderedImage);
	m_renderedImage = SDL_CreateSurfaceFrom(m_width, m_height, SDL_PIXELFORMAT_ABGR8888, downloadedData, m_width * 4);

	SDL_Surface* convertedRender = SDL_ConvertSurface(m_renderedImage, SDL_PIXELFORMAT_RGBA8888);
	SDL_DestroySurface(m_renderedImage);
	SDL_UnmapGPUTransferBuffer(m_device, m_downloadTransferBuffer);
	m_renderedImage = convertedRender;
	SDL_BlitSurface(m_renderedImage, nullptr, DDBackBuffer, nullptr);

	return DD_OK;
}
