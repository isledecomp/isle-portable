#include "ShaderIndex.h"
#include "d3drmrenderer_sdl3gpu.h"
#include "ddraw_impl.h"
#include "miniwin.h"

#include <SDL3/SDL.h>

static SDL_GPUGraphicsPipeline* InitializeGraphicsPipeline(SDL_GPUDevice* device)
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
	vertexBufferDescs[0].pitch = sizeof(PositionColorVertex);
	vertexBufferDescs[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
	vertexBufferDescs[0].instance_step_rate = 0;

	SDL_GPUVertexAttribute vertexAttrs[3] = {};
	vertexAttrs[0].location = 0;
	vertexAttrs[0].buffer_slot = 0;
	vertexAttrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	vertexAttrs[0].offset = 0;

	vertexAttrs[1].location = 1;
	vertexAttrs[1].buffer_slot = 0;
	vertexAttrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	vertexAttrs[1].offset = sizeof(float) * 3;

	vertexAttrs[2].location = 2;
	vertexAttrs[2].buffer_slot = 0;
	vertexAttrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
	vertexAttrs[2].offset = sizeof(float) * 6;

	SDL_GPUVertexInputState vertexInputState = {};
	vertexInputState.vertex_buffer_descriptions = vertexBufferDescs;
	vertexInputState.num_vertex_buffers = SDL_arraysize(vertexBufferDescs);
	vertexInputState.vertex_attributes = vertexAttrs;
	vertexInputState.num_vertex_attributes = SDL_arraysize(vertexAttrs);

	SDL_GPUColorTargetDescription colorTargets = {};
	colorTargets.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

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
	pipelineCreateInfo.depth_stencil_state.enable_depth_write = true;
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
	SDL_GPUGraphicsPipeline* pipeline = InitializeGraphicsPipeline(device);
	if (!pipeline) {
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
		SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
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
		SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
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
		SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
		SDL_ReleaseGPUTexture(device, depthTexture);
		SDL_ReleaseGPUTexture(device, transferTexture);
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_CreateGPUTransferBuffer failed (%s)", SDL_GetError());
		return nullptr;
	}

	return new Direct3DRMSDL3GPURenderer(
		width,
		height,
		device,
		pipeline,
		transferTexture,
		depthTexture,
		downloadTransferBuffer
	);
}

Direct3DRMSDL3GPURenderer::Direct3DRMSDL3GPURenderer(
	DWORD width,
	DWORD height,
	SDL_GPUDevice* device,
	SDL_GPUGraphicsPipeline* pipeline,
	SDL_GPUTexture* transferTexture,
	SDL_GPUTexture* depthTexture,
	SDL_GPUTransferBuffer* downloadTransferBuffer
)
	: m_width(width), m_height(height), m_device(device), m_pipeline(pipeline), m_transferTexture(transferTexture),
	  m_depthTexture(depthTexture), m_downloadTransferBuffer(downloadTransferBuffer)
{
}

Direct3DRMSDL3GPURenderer::~Direct3DRMSDL3GPURenderer()
{
	SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer);
	SDL_ReleaseGPUTransferBuffer(m_device, m_downloadTransferBuffer);
	SDL_ReleaseGPUTexture(m_device, m_depthTexture);
	SDL_ReleaseGPUTexture(m_device, m_transferTexture);
	SDL_ReleaseGPUGraphicsPipeline(m_device, m_pipeline);
	SDL_DestroyGPUDevice(m_device);
}

void Direct3DRMSDL3GPURenderer::SetBackbuffer(SDL_Surface* buf)
{
	m_backbuffer = buf;
}

void Direct3DRMSDL3GPURenderer::PushLights(const SceneLight* vertices, size_t count)
{
	if (count > 3) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "Unsupported number of lights (%d)", count);
		count = 3;
	}
	memcpy(&m_lights.lights, vertices, sizeof(SceneLight) * count);
	m_lights.count = count;
}

void Direct3DRMSDL3GPURenderer::PushVertices(const PositionColorVertex* vertices, size_t count)
{
	if (count > m_vertexBufferCount) {
		if (m_vertexBuffer) {
			SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer);
		}
		SDL_GPUBufferCreateInfo bufferCreateInfo = {};
		bufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
		bufferCreateInfo.size = static_cast<Uint32>(sizeof(PositionColorVertex) * count);
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
	transferCreateInfo.size = static_cast<Uint32>(sizeof(PositionColorVertex) * m_vertexCount);
	SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(m_device, &transferCreateInfo);
	if (!transferBuffer) {
		SDL_LogError(
			LOG_CATEGORY_MINIWIN,
			"SDL_CreateGPUTransferBuffer returned NULL transfer buffer (%s)",
			SDL_GetError()
		);
	}

	PositionColorVertex* transferData =
		(PositionColorVertex*) SDL_MapGPUTransferBuffer(m_device, transferBuffer, false);
	if (!transferData) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_MapGPUTransferBuffer returned NULL buffer (%s)", SDL_GetError());
	}

	memcpy(transferData, vertices, m_vertexCount * sizeof(PositionColorVertex));

	SDL_UnmapGPUTransferBuffer(m_device, transferBuffer);

	// Upload the transfer data to the vertex buffer
	SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(m_device);
	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);
	SDL_GPUTransferBufferLocation transferLocation = {};
	transferLocation.transfer_buffer = transferBuffer;
	transferLocation.offset = 0;

	SDL_GPUBufferRegion bufferRegion = {};
	bufferRegion.buffer = m_vertexBuffer;
	bufferRegion.offset = 0;
	bufferRegion.size = static_cast<Uint32>(sizeof(PositionColorVertex) * m_vertexCount);

	SDL_UploadToGPUBuffer(copyPass, &transferLocation, &bufferRegion, false);

	SDL_EndGPUCopyPass(copyPass);
	if (!SDL_SubmitGPUCommandBuffer(uploadCmdBuf)) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_SubmitGPUCommandBuffer failes (%s)", SDL_GetError());
	}
	SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);
}

void Direct3DRMSDL3GPURenderer::SetProjection(D3DRMMATRIX4D perspective, D3DVALUE front, D3DVALUE back)
{
	m_front = front;
	m_back = back;
	memcpy(&m_uniforms.perspective, perspective, sizeof(D3DRMMATRIX4D));
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
	return "SDL3 GPU Rendere";
}

HRESULT Direct3DRMSDL3GPURenderer::Render()
{
	SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(m_device);
	if (cmdbuf == NULL) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_AcquireGPUCommandBuffer failed (%s)", SDL_GetError());
		return DDERR_GENERIC;
	}

	// Render the graphics
	SDL_GPUColorTargetInfo colorTargetInfo = {};
	colorTargetInfo.texture = m_transferTexture;
	// Make the render target transparent so we can combine it with the back buffer
	colorTargetInfo.clear_color = {0, 0, 0, 0};
	colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;

	SDL_GPUDepthStencilTargetInfo depthStencilTargetInfo = {};
	depthStencilTargetInfo.texture = m_depthTexture;
	depthStencilTargetInfo.clear_depth = 0.f;
	depthStencilTargetInfo.clear_stencil = 0;
	depthStencilTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
	depthStencilTargetInfo.store_op = SDL_GPU_STOREOP_STORE;
	depthStencilTargetInfo.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
	depthStencilTargetInfo.stencil_store_op = SDL_GPU_STOREOP_STORE;

	SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, &depthStencilTargetInfo);
	SDL_BindGPUGraphicsPipeline(renderPass, m_pipeline);

	SDL_PushGPUVertexUniformData(cmdbuf, 0, &m_uniforms, sizeof(m_uniforms));
	SDL_PushGPUFragmentUniformData(cmdbuf, 0, &m_lights, sizeof(m_lights));

	if (m_vertexCount) {
		SDL_GPUBufferBinding vertexBufferBinding = {};
		vertexBufferBinding.buffer = m_vertexBuffer;
		vertexBufferBinding.offset = 0;
		SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBufferBinding, 1);
		SDL_DrawGPUPrimitives(renderPass, m_vertexCount, 1, 0, 0);
	}

	SDL_EndGPURenderPass(renderPass);

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
	return Blit();
}

HRESULT Direct3DRMSDL3GPURenderer::Blit()
{
	// Blit the render back to our backbuffer
	SDL_Rect srcRect{0, 0, (int) m_width, (int) m_height};

	const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(m_backbuffer->format);
	if (details->Amask != 0) {
		// Backbuffer supports transparnacy
		SDL_Surface* convertedRender = SDL_ConvertSurface(m_renderedImage, m_backbuffer->format);
		SDL_DestroySurface(m_renderedImage);
		m_renderedImage = convertedRender;
		return DD_OK;
	}

	if (m_renderedImage->format == m_backbuffer->format) {
		// No conversion needed
		SDL_BlitSurface(m_renderedImage, &srcRect, m_backbuffer, &srcRect);
		return DD_OK;
	}

	// Convert backbuffer to a format that supports transparancy
	SDL_Surface* tempBackbuffer = SDL_ConvertSurface(m_backbuffer, m_renderedImage->format);
	SDL_BlitSurface(m_renderedImage, &srcRect, tempBackbuffer, &srcRect);
	// Then convert the result back to the backbuffer format and write it back
	SDL_Surface* newBackBuffer = SDL_ConvertSurface(tempBackbuffer, m_backbuffer->format);
	SDL_DestroySurface(tempBackbuffer);
	SDL_BlitSurface(newBackBuffer, &srcRect, m_backbuffer, &srcRect);
	SDL_DestroySurface(newBackBuffer);
	return DD_OK;
}
