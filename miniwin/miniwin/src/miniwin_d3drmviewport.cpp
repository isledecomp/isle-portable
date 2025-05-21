#include "miniwin_d3drm_p.h"
#include "miniwin_d3drmviewport_p.h"
#include "miniwin_p.h"

#include <SDL3/SDL.h>

Direct3DRMViewportImpl::Direct3DRMViewportImpl(
	DWORD width,
	DWORD height,
	SDL_GPUDevice* device,
	SDL_GPUTexture* transferTexture,
	SDL_GPUTransferBuffer* downloadTransferBuffer,
	SDL_GPUGraphicsPipeline* pipeline
)
	: m_width(width), m_height(height), m_device(device), m_transferTexture(transferTexture),
	  m_downloadTransferBuffer(downloadTransferBuffer), m_pipeline(pipeline)
{
}

void Direct3DRMViewportImpl::FreeDeviceResources()
{
	SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer);
	SDL_ReleaseGPUGraphicsPipeline(m_device, m_pipeline);
}

Direct3DRMViewportImpl::~Direct3DRMViewportImpl()
{
	FreeDeviceResources();
}

void Direct3DRMViewportImpl::Update()
{
	m_vertexCount = 3;

	SDL_GPUBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
	bufferCreateInfo.size = static_cast<Uint32>(sizeof(PositionColorVertex) * m_vertexCount);

	m_vertexBuffer = SDL_CreateGPUBuffer(m_device, &bufferCreateInfo);

	MINIWIN_NOT_IMPLEMENTED();
	SDL_GPUTransferBufferCreateInfo transferCreateInfo = {};
	transferCreateInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	transferCreateInfo.size = static_cast<Uint32>(sizeof(PositionColorVertex) * m_vertexCount);

	SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(m_device, &transferCreateInfo);

	PositionColorVertex* transferData =
		(PositionColorVertex*) SDL_MapGPUTransferBuffer(m_device, transferBuffer, false);

	transferData[0] = {-1, -1, 0, 255, 0, 0, 255};
	transferData[1] = {1, -1, 0, 0, 0, 255, 255};
	transferData[2] = {0, 1, 0, 0, 255, 0, 128};

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
	SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
	SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);

	m_updated = true;
}

HRESULT Direct3DRMViewportImpl::Render(IDirect3DRMFrame* group)
{
	if (!m_updated) {
		return DDERR_GENERIC;
	}

	if (!m_device) {
		return DDERR_GENERIC;
	}

	SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(m_device);
	if (cmdbuf == NULL) {
		return DDERR_GENERIC;
	}

	// Render the graphics
	SDL_GPUColorTargetInfo colorTargetInfo = {};
	colorTargetInfo.texture = m_transferTexture;
	// Make the render target transparent so we can combine it with the back buffer
	colorTargetInfo.clear_color = {0, 0, 0, 0};
	colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
	SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, NULL);
	SDL_BindGPUGraphicsPipeline(renderPass, m_pipeline);
	SDL_GPUBufferBinding vertexBufferBinding = {};
	vertexBufferBinding.buffer = m_vertexBuffer;
	vertexBufferBinding.offset = 0;
	SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBufferBinding, 1);
	SDL_DrawGPUPrimitives(renderPass, m_vertexCount, 1, 0, 0);
	SDL_EndGPURenderPass(renderPass);

	// Download rendered image
	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdbuf);
	SDL_GPUTextureRegion region = {};
	region.texture = m_transferTexture;
	region.w = DDBackBuffer->w;
	region.h = DDBackBuffer->h;
	region.d = 1;
	SDL_GPUTextureTransferInfo transferInfo = {};
	transferInfo.transfer_buffer = m_downloadTransferBuffer;
	transferInfo.offset = 0;
	SDL_DownloadFromGPUTexture(copyPass, &region, &transferInfo);
	SDL_EndGPUCopyPass(copyPass);
	SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmdbuf);
	if (!cmdbuf || !SDL_WaitForGPUFences(m_device, true, &fence, 1)) {
		return DDERR_GENERIC;
	}
	SDL_ReleaseGPUFence(m_device, fence);
	void* downloadedData = SDL_MapGPUTransferBuffer(m_device, m_downloadTransferBuffer, false);
	if (!downloadedData) {
		return DDERR_GENERIC;
	}

	m_updated = false;

	SDL_DestroySurface(m_renderedImage);
	m_renderedImage = SDL_CreateSurfaceFrom(
		DDBackBuffer->w,
		DDBackBuffer->h,
		SDL_PIXELFORMAT_ABGR8888,
		downloadedData,
		DDBackBuffer->w * 4
	);

	SDL_Surface* convertedRender = SDL_ConvertSurface(m_renderedImage, SDL_PIXELFORMAT_RGBA8888);
	SDL_DestroySurface(m_renderedImage);
	SDL_UnmapGPUTransferBuffer(m_device, m_downloadTransferBuffer);
	m_renderedImage = convertedRender;

	return ForceUpdate(0, 0, DDBackBuffer->w, DDBackBuffer->h);
}

HRESULT Direct3DRMViewportImpl::ForceUpdate(int x, int y, int w, int h)
{
	if (!m_renderedImage) {
		return DDERR_GENERIC;
	}
	// Blit the render back to our backbuffer
	SDL_Rect srcRect{0, 0, DDBackBuffer->w, DDBackBuffer->h};

	const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(DDBackBuffer->format);
	if (details->Amask != 0) {
		// Backbuffer supports transparnacy
		SDL_Surface* convertedRender = SDL_ConvertSurface(m_renderedImage, DDBackBuffer->format);
		SDL_DestroySurface(m_renderedImage);
		m_renderedImage = convertedRender;
		return DD_OK;
	}

	if (m_renderedImage->format == DDBackBuffer->format) {
		// No conversion needed
		SDL_BlitSurface(m_renderedImage, &srcRect, DDBackBuffer, &srcRect);
		return DD_OK;
	}

	// Convert backbuffer to a format that supports transparancy
	SDL_Surface* tempBackbuffer = SDL_ConvertSurface(DDBackBuffer, m_renderedImage->format);
	SDL_BlitSurface(m_renderedImage, &srcRect, tempBackbuffer, &srcRect);
	// Then convert the result back to the backbuffer format and write it back
	SDL_Surface* newBackBuffer = SDL_ConvertSurface(tempBackbuffer, DDBackBuffer->format);
	SDL_DestroySurface(tempBackbuffer);
	SDL_BlitSurface(newBackBuffer, &srcRect, DDBackBuffer, &srcRect);
	SDL_DestroySurface(newBackBuffer);
	return DD_OK;
}

HRESULT Direct3DRMViewportImpl::Clear()
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMViewportImpl::SetCamera(IDirect3DRMFrame* camera)
{
	if (m_camera) {
		m_camera->Release();
	}
	if (camera) {
		camera->AddRef();
	}
	m_camera = camera;
	return DD_OK;
}

HRESULT Direct3DRMViewportImpl::GetCamera(IDirect3DRMFrame** camera)
{
	if (m_camera) {
		m_camera->AddRef();
	}
	*camera = m_camera;
	return DD_OK;
}

HRESULT Direct3DRMViewportImpl::SetProjection(D3DRMPROJECTIONTYPE type)
{
	return DD_OK;
}

D3DRMPROJECTIONTYPE Direct3DRMViewportImpl::GetProjection()
{
	return D3DRMPROJECTIONTYPE::PERSPECTIVE;
}

HRESULT Direct3DRMViewportImpl::SetFront(D3DVALUE z)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

D3DVALUE Direct3DRMViewportImpl::GetFront()
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

HRESULT Direct3DRMViewportImpl::SetBack(D3DVALUE z)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

D3DVALUE Direct3DRMViewportImpl::GetBack()
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

HRESULT Direct3DRMViewportImpl::SetField(D3DVALUE field)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

D3DVALUE Direct3DRMViewportImpl::GetField()
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

DWORD Direct3DRMViewportImpl::GetWidth()
{
	return m_width;
}

DWORD Direct3DRMViewportImpl::GetHeight()
{
	return m_height;
}

HRESULT Direct3DRMViewportImpl::Transform(D3DRMVECTOR4D* screen, D3DVECTOR* world)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMViewportImpl::InverseTransform(D3DVECTOR* world, D3DRMVECTOR4D* screen)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMViewportImpl::Pick(float x, float y, LPDIRECT3DRMPICKEDARRAY* pickedArray)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

void Direct3DRMViewportImpl::CloseDevice()
{
	FreeDeviceResources();
	m_device = nullptr;
}
