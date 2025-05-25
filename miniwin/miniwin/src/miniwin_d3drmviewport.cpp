#include "miniwin_d3drm_p.h"
#include "miniwin_d3drmframe_p.h"
#include "miniwin_d3drmviewport_p.h"
#include "miniwin_p.h"

#include <SDL3/SDL.h>
#include <functional>

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

HRESULT Direct3DRMViewportImpl::CollectSceneData(IDirect3DRMFrame* group)
{
	MINIWIN_NOT_IMPLEMENTED(); // Lights, textures, materials

	std::vector<PositionColorVertex> verts;
	std::function<void(IDirect3DRMFrame*)> recurseFrame;

	recurseFrame = [&](IDirect3DRMFrame* frame) {
		IDirect3DRMVisualArray* va = nullptr;
		if (SUCCEEDED(frame->GetVisuals(&va)) && va) {
			DWORD n = va->GetSize();
			for (DWORD i = 0; i < n; ++i) {
				IDirect3DRMVisual* vis = nullptr;
				va->GetElement(i, &vis);
				if (!vis) {
					continue;
				}

				// Pull geometry from meshes
				IDirect3DRMMesh* mesh = nullptr;
				if (SUCCEEDED(vis->QueryInterface(IID_IDirect3DRMMesh, (void**) &mesh)) && mesh) {
					DWORD groupCount = mesh->GetGroupCount();
					for (DWORD gi = 0; gi < groupCount; ++gi) {
						DWORD vtxCount, faceCount, vpf, dataSize;
						mesh->GetGroup(gi, &vtxCount, &faceCount, &vpf, &dataSize, nullptr);
						std::vector<D3DRMVERTEX> d3dVerts(vtxCount);
						std::vector<DWORD> faces(faceCount * vpf);
						mesh->GetVertices(gi, 0, vtxCount, d3dVerts.data());
						mesh->GetGroup(gi, &vtxCount, &faceCount, &vpf, nullptr, faces.data());
						D3DCOLOR color = mesh->GetGroupColor(gi);

						for (DWORD fi = 0; fi < faceCount; ++fi) {
							for (int idx = 0; idx < vpf; ++idx) {
								auto& dv = d3dVerts[faces[fi * vpf + idx]];
								PositionColorVertex vtx;
								vtx.x = dv.position.x;
								vtx.y = dv.position.y;
								vtx.z = dv.position.z;
								vtx.a = (color >> 24) & 0xFF;
								vtx.r = (color >> 16) & 0xFF;
								vtx.g = (color >> 8) & 0xFF;
								vtx.b = (color >> 0) & 0xFF;
								verts.push_back(vtx);
							}
						}
					}
					mesh->Release();
				}

				// Recurse over sub-frames
				IDirect3DRMFrame* childFrame = nullptr;
				if (SUCCEEDED(vis->QueryInterface(IID_IDirect3DRMFrame, (void**) &childFrame)) && childFrame) {
					recurseFrame(childFrame);
					childFrame->Release();
				}

				vis->Release();
			}
			va->Release();
		}
	};

	recurseFrame(group);

	PushVertices(verts.data(), verts.size());

	return D3DRM_OK;
}

void Direct3DRMViewportImpl::PushVertices(const PositionColorVertex* vertices, size_t count)
{
	if (count > m_vertexBufferCount) {
		if (m_vertexBuffer) {
			SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer);
		}
		SDL_GPUBufferCreateInfo bufferCreateInfo = {};
		bufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
		bufferCreateInfo.size = static_cast<Uint32>(sizeof(PositionColorVertex) * count);
		m_vertexBuffer = SDL_CreateGPUBuffer(m_device, &bufferCreateInfo);
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

	PositionColorVertex* transferData =
		(PositionColorVertex*) SDL_MapGPUTransferBuffer(m_device, transferBuffer, false);

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
	SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
	SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);
}

HRESULT Direct3DRMViewportImpl::Render(IDirect3DRMFrame* group)
{
	if (!m_device) {
		return DDERR_GENERIC;
	}

	HRESULT success = CollectSceneData(group);
	if (success != DD_OK) {
		return success;
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
	region.w = DDBackBuffer->w;
	region.h = DDBackBuffer->h;
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
	if (!DDBackBuffer) {
		return DDERR_GENERIC;
	}

	uint8_t r = (m_backgroundColor >> 16) & 0xFF;
	uint8_t g = (m_backgroundColor >> 8) & 0xFF;
	uint8_t b = m_backgroundColor & 0xFF;

	Uint32 color = SDL_MapRGB(SDL_GetPixelFormatDetails(DDBackBuffer->format), nullptr, r, g, b);
	SDL_FillSurfaceRect(DDBackBuffer, NULL, color);
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
	m_zMin = z;
	return DD_OK;
}

D3DVALUE Direct3DRMViewportImpl::GetFront()
{
	return m_zMin;
}

HRESULT Direct3DRMViewportImpl::SetBack(D3DVALUE z)
{
	m_zMax = z;
	return DD_OK;
}

D3DVALUE Direct3DRMViewportImpl::GetBack()
{
	return m_zMax;
}

HRESULT Direct3DRMViewportImpl::SetField(D3DVALUE field)
{
	m_fov = field;
	return DD_OK;
}

D3DVALUE Direct3DRMViewportImpl::GetField()
{
	return m_fov;
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
