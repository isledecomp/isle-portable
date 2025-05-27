#include "SDL3/SDL_stdinc.h"
#include "miniwin_d3drm_sdl3gpu.h"
#include "miniwin_d3drmframe_sdl3gpu.h"
#include "miniwin_d3drmviewport_sdl3gpu.h"
#include "miniwin_p.h"

#include <SDL3/SDL.h>
#include <cassert>
#include <float.h>
#include <functional>
#include <math.h>

Direct3DRMViewport_SDL3GPUImpl::Direct3DRMViewport_SDL3GPUImpl(
	DWORD width,
	DWORD height,
	SDL_GPUDevice* device,
	SDL_GPUTexture* transferTexture,
	SDL_GPUTexture* depthTexture,
	SDL_GPUTransferBuffer* downloadTransferBuffer,
	SDL_GPUGraphicsPipeline* pipeline
)
	: m_width(width), m_height(height), m_device(device), m_transferTexture(transferTexture),
	  m_depthTexture(depthTexture), m_downloadTransferBuffer(downloadTransferBuffer), m_pipeline(pipeline)
{
}

void Direct3DRMViewport_SDL3GPUImpl::FreeDeviceResources()
{
	SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer);
	SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer);
	SDL_ReleaseGPUGraphicsPipeline(m_device, m_pipeline);
}

Direct3DRMViewport_SDL3GPUImpl::~Direct3DRMViewport_SDL3GPUImpl()
{
	FreeDeviceResources();
}

static void D3DRMMatrixMultiply(D3DRMMATRIX4D out, const D3DRMMATRIX4D a, const D3DRMMATRIX4D b)
{
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			out[i][j] = 0.0f;
			for (int k = 0; k < 4; ++k) {
				out[i][j] += a[i][k] * b[k][j];
			}
		}
	}
}

static D3DVALUE Cofactor3x3(const D3DRMMATRIX4D m, int i, int j)
{
	int i1 = (i + 1) % 3;
	int i2 = (i + 1) % 3;
	int j1 = (j + 1) % 3;
	int j2 = (j + 1) % 3;
	return m[i1][j1] * m[i2][j2] - m[i2][j1] * m[i1][j2];
}

static void D3DRMMatrixInvertForNormal(D3DRMMATRIX3D out, const D3DRMMATRIX4D m)
{
	assert(m[3][3] == 1.f);
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			out[i][j] = (((i + j) % 2) ? -1 : 1) * Cofactor3x3(m, i, j);
		}
	}
}

static void D3DRMMatrixInvertOrthogonal(D3DRMMATRIX4D out, const D3DRMMATRIX4D m)
{
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			out[i][j] = m[j][i];
		}
	}

	out[0][3] = out[1][3] = out[2][3] = 0.f;
	out[3][3] = 1.f;

	D3DVECTOR t = {m[3][0], m[3][1], m[3][2]};

	out[3][0] = -(out[0][0] * t.x + out[1][0] * t.y + out[2][0] * t.z);
	out[3][1] = -(out[0][1] * t.x + out[1][1] * t.y + out[2][1] * t.z);
	out[3][2] = -(out[0][2] * t.x + out[1][2] * t.y + out[2][2] * t.z);
}

static void CalculateProjectionMatrix(D3DRMMATRIX4D Result, float field, float aspect, float near, float far)
{
	float f = near / field;
	float depth = far - near;

	D3DRMMATRIX4D perspective = {
		{f, 0, 0, 0},
		{0, f * aspect, 0, 0},
		{0, 0, far / depth, 1},
		{0, 0, (-near * far) / depth, 0},
	};

	memcpy(Result, &perspective, sizeof(D3DRMMATRIX4D));
}

static void ComputeFrameWorldMatrix(IDirect3DRMFrame* frame, D3DRMMATRIX4D out)
{
	D3DRMMATRIX4D acc = {{1.f, 0.f, 0.f, 0.f}, {0.f, 1.f, 0.f, 0.f}, {0.f, 0.f, 1.f, 0.f}, {0.f, 0.f, 0.f, 1.f}};

	IDirect3DRMFrame* cur = frame;
	while (cur) {
		auto* impl = static_cast<Direct3DRMFrame_SDL3GPUImpl*>(cur);
		D3DRMMATRIX4D local;
		memcpy(local, impl->m_transform, sizeof(local));

		D3DRMMATRIX4D tmp;
		D3DRMMatrixMultiply(tmp, local, acc);
		memcpy(acc, tmp, sizeof(acc));

		if (cur == impl->m_parent) {
			break;
		}
		cur = impl->m_parent;
	}
	memcpy(out, acc, sizeof(acc));
}

HRESULT Direct3DRMViewport_SDL3GPUImpl::CollectSceneData()
{
	MINIWIN_NOT_IMPLEMENTED(); // Lights, camera, textures, materials

	std::vector<PositionColorVertex> verts;

	// Compute camera matrix
	D3DRMMATRIX4D cameraWorld, viewMatrix;
	ComputeFrameWorldMatrix(m_camera, cameraWorld);
	D3DRMMatrixInvertOrthogonal(viewMatrix, cameraWorld);

	std::function<void(IDirect3DRMFrame*, D3DRMMATRIX4D)> recurseFrame;

	recurseFrame = [&](IDirect3DRMFrame* frame, D3DRMMATRIX4D parentMatrix) {
		// Retrieve the current frame's transform
		Direct3DRMFrame_SDL3GPUImpl* frameImpl = static_cast<Direct3DRMFrame_SDL3GPUImpl*>(frame);
		D3DRMMATRIX4D localMatrix;
		memcpy(localMatrix, frameImpl->m_transform, sizeof(D3DRMMATRIX4D));

		// Compute combined world matrix: world = parent * local
		D3DRMMATRIX4D worldMatrix;
		D3DRMMATRIX3D worldMatrixInvert;
		D3DRMMatrixMultiply(worldMatrix, parentMatrix, localMatrix);
		D3DRMMatrixInvertForNormal(worldMatrixInvert, worldMatrix);

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

								// Apply world transform to the vertex
								D3DVECTOR pos = dv.position;
								D3DVECTOR norm = dv.normal;
								D3DVECTOR worldPos;
								worldPos.x = pos.x * worldMatrix[0][0] + pos.y * worldMatrix[1][0] +
											 pos.z * worldMatrix[2][0] + worldMatrix[3][0];
								worldPos.y = pos.x * worldMatrix[0][1] + pos.y * worldMatrix[1][1] +
											 pos.z * worldMatrix[2][1] + worldMatrix[3][1];
								worldPos.z = pos.x * worldMatrix[0][2] + pos.y * worldMatrix[1][2] +
											 pos.z * worldMatrix[2][2] + worldMatrix[3][2];

								// View transform
								D3DVECTOR viewPos;
								viewPos.x = worldPos.x * viewMatrix[0][0] + worldPos.y * viewMatrix[1][0] +
											worldPos.z * viewMatrix[2][0] + viewMatrix[3][0];
								viewPos.y = worldPos.x * viewMatrix[0][1] + worldPos.y * viewMatrix[1][1] +
											worldPos.z * viewMatrix[2][1] + viewMatrix[3][1];
								viewPos.z = worldPos.x * viewMatrix[0][2] + worldPos.y * viewMatrix[1][2] +
											worldPos.z * viewMatrix[2][2] + viewMatrix[3][2];

								// View transform
								D3DVECTOR viewNorm;
								viewNorm.x = norm.x * worldMatrixInvert[0][0] + norm.y * worldMatrixInvert[1][0] +
											 norm.z * worldMatrixInvert[2][0];
								viewNorm.y = norm.x * worldMatrixInvert[0][1] + norm.y * worldMatrixInvert[1][1] +
											 norm.z * worldMatrixInvert[2][1];
								viewNorm.z = norm.x * worldMatrixInvert[0][2] + norm.y * worldMatrixInvert[1][2] +
											 norm.z * worldMatrixInvert[2][2];

								PositionColorVertex vtx;
								vtx.x = viewPos.x;
								vtx.y = viewPos.y;
								vtx.z = viewPos.z;
								vtx.nx = viewNorm.x;
								vtx.ny = viewNorm.y;
								vtx.nz = viewNorm.z;
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

				// Recurse into child frames
				IDirect3DRMFrame* childFrame = nullptr;
				if (SUCCEEDED(vis->QueryInterface(IID_IDirect3DRMFrame, (void**) &childFrame)) && childFrame) {
					recurseFrame(childFrame, worldMatrix);
					childFrame->Release();
				}

				vis->Release();
			}
			va->Release();
		}
	};

	D3DRMMATRIX4D identity = {{1.f, 0.f, 0.f, 0.f}, {0.f, 1.f, 0.f, 0.f}, {0.f, 0.f, 1.f, 0.f}, {0.f, 0.f, 0.f, 1.f}};

	recurseFrame(m_rootFrame, identity);

	PushVertices(verts.data(), verts.size());

	CalculateProjectionMatrix(m_uniforms.perspective, m_field, (float) m_width / (float) m_height, m_front, m_back);

	return D3DRM_OK;
}

void Direct3DRMViewport_SDL3GPUImpl::PushVertices(const PositionColorVertex* vertices, size_t count)
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

HRESULT Direct3DRMViewport_SDL3GPUImpl::Render(IDirect3DRMFrame* rootFrame)
{
	if (!m_device) {
		return DDERR_GENERIC;
	}

	m_rootFrame = rootFrame;
	HRESULT success = CollectSceneData();
	if (success != DD_OK) {
		return success;
	}

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
	region.w = DDBackBuffer_SDL3GPU->w;
	region.h = DDBackBuffer_SDL3GPU->h;
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
		DDBackBuffer_SDL3GPU->w,
		DDBackBuffer_SDL3GPU->h,
		SDL_PIXELFORMAT_ABGR8888,
		downloadedData,
		DDBackBuffer_SDL3GPU->w * 4
	);

	SDL_Surface* convertedRender = SDL_ConvertSurface(m_renderedImage, SDL_PIXELFORMAT_RGBA8888);
	SDL_DestroySurface(m_renderedImage);
	SDL_UnmapGPUTransferBuffer(m_device, m_downloadTransferBuffer);
	m_renderedImage = convertedRender;

	return ForceUpdate(0, 0, DDBackBuffer_SDL3GPU->w, DDBackBuffer_SDL3GPU->h);
}

HRESULT Direct3DRMViewport_SDL3GPUImpl::ForceUpdate(int x, int y, int w, int h)
{
	if (!m_renderedImage) {
		return DDERR_GENERIC;
	}
	// Blit the render back to our backbuffer
	SDL_Rect srcRect{0, 0, DDBackBuffer_SDL3GPU->w, DDBackBuffer_SDL3GPU->h};

	const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(DDBackBuffer_SDL3GPU->format);
	if (details->Amask != 0) {
		// Backbuffer supports transparnacy
		SDL_Surface* convertedRender = SDL_ConvertSurface(m_renderedImage, DDBackBuffer_SDL3GPU->format);
		SDL_DestroySurface(m_renderedImage);
		m_renderedImage = convertedRender;
		return DD_OK;
	}

	if (m_renderedImage->format == DDBackBuffer_SDL3GPU->format) {
		// No conversion needed
		SDL_BlitSurface(m_renderedImage, &srcRect, DDBackBuffer_SDL3GPU, &srcRect);
		return DD_OK;
	}

	// Convert backbuffer to a format that supports transparancy
	SDL_Surface* tempBackbuffer = SDL_ConvertSurface(DDBackBuffer_SDL3GPU, m_renderedImage->format);
	SDL_BlitSurface(m_renderedImage, &srcRect, tempBackbuffer, &srcRect);
	// Then convert the result back to the backbuffer format and write it back
	SDL_Surface* newBackBuffer = SDL_ConvertSurface(tempBackbuffer, DDBackBuffer_SDL3GPU->format);
	SDL_DestroySurface(tempBackbuffer);
	SDL_BlitSurface(newBackBuffer, &srcRect, DDBackBuffer_SDL3GPU, &srcRect);
	SDL_DestroySurface(newBackBuffer);
	return DD_OK;
}

HRESULT Direct3DRMViewport_SDL3GPUImpl::Clear()
{
	if (!DDBackBuffer_SDL3GPU) {
		return DDERR_GENERIC;
	}

	uint8_t r = (m_backgroundColor >> 16) & 0xFF;
	uint8_t g = (m_backgroundColor >> 8) & 0xFF;
	uint8_t b = m_backgroundColor & 0xFF;

	Uint32 color = SDL_MapRGB(SDL_GetPixelFormatDetails(DDBackBuffer_SDL3GPU->format), nullptr, r, g, b);
	SDL_FillSurfaceRect(DDBackBuffer_SDL3GPU, NULL, color);
	return DD_OK;
}

HRESULT Direct3DRMViewport_SDL3GPUImpl::SetCamera(IDirect3DRMFrame* camera)
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

HRESULT Direct3DRMViewport_SDL3GPUImpl::GetCamera(IDirect3DRMFrame** camera)
{
	if (m_camera) {
		m_camera->AddRef();
	}
	*camera = m_camera;
	return DD_OK;
}

HRESULT Direct3DRMViewport_SDL3GPUImpl::SetProjection(D3DRMPROJECTIONTYPE type)
{
	return DD_OK;
}

D3DRMPROJECTIONTYPE Direct3DRMViewport_SDL3GPUImpl::GetProjection()
{
	return D3DRMPROJECTIONTYPE::PERSPECTIVE;
}

HRESULT Direct3DRMViewport_SDL3GPUImpl::SetFront(D3DVALUE z)
{
	m_front = z;
	return DD_OK;
}

D3DVALUE Direct3DRMViewport_SDL3GPUImpl::GetFront()
{
	return m_front;
}

HRESULT Direct3DRMViewport_SDL3GPUImpl::SetBack(D3DVALUE z)
{
	m_back = z;
	return DD_OK;
}

D3DVALUE Direct3DRMViewport_SDL3GPUImpl::GetBack()
{
	return m_back;
}

HRESULT Direct3DRMViewport_SDL3GPUImpl::SetField(D3DVALUE field)
{
	m_field = field;
	return DD_OK;
}

D3DVALUE Direct3DRMViewport_SDL3GPUImpl::GetField()
{
	return m_field;
}

DWORD Direct3DRMViewport_SDL3GPUImpl::GetWidth()
{
	return m_width;
}

DWORD Direct3DRMViewport_SDL3GPUImpl::GetHeight()
{
	return m_height;
}

HRESULT Direct3DRMViewport_SDL3GPUImpl::Transform(D3DRMVECTOR4D* screen, D3DVECTOR* world)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMViewport_SDL3GPUImpl::InverseTransform(D3DVECTOR* world, D3DRMVECTOR4D* screen)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

struct Ray {
	D3DVECTOR origin;
	D3DVECTOR direction;
};

// Ray-box intersection: slab method
bool RayIntersectsBox(const Ray& ray, const D3DRMBOX& box, float& outT)
{
	float tmin = -FLT_MAX;
	float tmax = FLT_MAX;

	for (int i = 0; i < 3; ++i) {
		float origin = (&ray.origin.x)[i];
		float dir = (&ray.direction.x)[i];
		float minB = (&box.min.x)[i];
		float maxB = (&box.max.x)[i];

		if (fabs(dir) < 1e-6f) {
			if (origin < minB || origin > maxB) {
				return false;
			}
		}
		else {
			float invD = 1.0f / dir;
			float t1 = (minB - origin) * invD;
			float t2 = (maxB - origin) * invD;
			if (t1 > t2) {
				std::swap(t1, t2);
			}
			if (t1 > tmin) {
				tmin = t1;
			}
			if (t2 < tmax) {
				tmax = t2;
			}
			if (tmin > tmax) {
				return false;
			}
			if (tmax < 0) {
				return false;
			}
		}
	}

	outT = tmin >= 0 ? tmin : tmax; // closest positive hit
	return true;
}

// Convert screen (x,y) in viewport to picking ray in world space
Ray BuildPickingRay(
	float x,
	float y,
	int width,
	int height,
	IDirect3DRMFrame* camera,
	float front,
	float back,
	float field,
	float aspect
)
{
	float nx = (2.0f * x) / width - 1.0f;
	float ny = 1.0f - (2.0f * y) / height; // flip Y since screen Y down

	D3DRMMATRIX4D proj;
	CalculateProjectionMatrix(proj, field, aspect, front, back);

	float f = front / field;

	// Ray in view space at near plane:
	D3DVECTOR rayDirView = {nx / f, ny / (f * aspect), 1.0f};

	// Normalize ray direction
	float len = sqrt(rayDirView.x * rayDirView.x + rayDirView.y * rayDirView.y + rayDirView.z * rayDirView.z);
	rayDirView.x /= len;
	rayDirView.y /= len;
	rayDirView.z /= len;

	// Compute camera world matrix and invert it to get view->world
	D3DRMMATRIX4D cameraWorld;
	ComputeFrameWorldMatrix(camera, cameraWorld);

	// Transform ray origin (camera position) and direction to world space
	D3DVECTOR rayOriginWorld = {cameraWorld[3][0], cameraWorld[3][1], cameraWorld[3][2]};
	// Multiply direction by rotation part of matrix only (3x3 upper-left)
	D3DVECTOR rayDirWorld = {
		rayDirView.x * cameraWorld[0][0] + rayDirView.y * cameraWorld[1][0] + rayDirView.z * cameraWorld[2][0],
		rayDirView.x * cameraWorld[0][1] + rayDirView.y * cameraWorld[1][1] + rayDirView.z * cameraWorld[2][1],
		rayDirView.x * cameraWorld[0][2] + rayDirView.y * cameraWorld[1][2] + rayDirView.z * cameraWorld[2][2]
	};

	len = sqrt(rayDirWorld.x * rayDirWorld.x + rayDirWorld.y * rayDirWorld.y + rayDirWorld.z * rayDirWorld.z);
	rayDirWorld.x /= len;
	rayDirWorld.y /= len;
	rayDirWorld.z /= len;

	return Ray{rayOriginWorld, rayDirWorld};
}

/**
 * @todo additionally check that we hit a triangle in the mesh
 */
HRESULT Direct3DRMViewport_SDL3GPUImpl::Pick(float x, float y, LPDIRECT3DRMPICKEDARRAY* pickedArray)
{
	if (!m_rootFrame) {
		return DDERR_GENERIC;
	}

	std::vector<PickRecord> hits;

	Ray pickRay = BuildPickingRay(
		x,
		y,
		m_width,
		m_height,
		m_camera,
		m_front,
		m_back,
		m_field,
		(float) m_width / (float) m_height
	);

	std::function<void(IDirect3DRMFrame*, std::vector<IDirect3DRMFrame*>&)> recurse;
	recurse = [&](IDirect3DRMFrame* frame, std::vector<IDirect3DRMFrame*>& path) {
		Direct3DRMFrame_SDL3GPUImpl* frameImpl = static_cast<Direct3DRMFrame_SDL3GPUImpl*>(frame);
		path.push_back(frame); // Push current frame

		IDirect3DRMVisualArray* visuals = nullptr;
		if (SUCCEEDED(frame->GetVisuals(&visuals)) && visuals) {
			DWORD count = visuals->GetSize();
			for (DWORD i = 0; i < count; ++i) {
				IDirect3DRMVisual* vis = nullptr;
				visuals->GetElement(i, &vis);

				IDirect3DRMMesh* mesh = nullptr;
				IDirect3DRMFrame* subFrame = nullptr;

				if (SUCCEEDED(vis->QueryInterface(IID_IDirect3DRMFrame, (void**) &subFrame)) && subFrame) {
					recurse(subFrame, path);
					subFrame->Release();
				}
				else if (SUCCEEDED(vis->QueryInterface(IID_IDirect3DRMMesh, (void**) &mesh)) && mesh) {
					D3DRMBOX box;
					if (SUCCEEDED(mesh->GetBox(&box))) {
						// Transform box corners to world space
						D3DRMMATRIX4D worldMatrix;
						ComputeFrameWorldMatrix(frame, worldMatrix);

						// Transform box min and max points
						// Because axis-aligned box can become oriented box after transform,
						// but we simplify by transforming all 8 corners and computing new AABB

						D3DVECTOR corners[8] = {
							{box.min.x, box.min.y, box.min.z},
							{box.min.x, box.min.y, box.max.z},
							{box.min.x, box.max.y, box.min.z},
							{box.min.x, box.max.y, box.max.z},
							{box.max.x, box.min.y, box.min.z},
							{box.max.x, box.min.y, box.max.z},
							{box.max.x, box.max.y, box.min.z},
							{box.max.x, box.max.y, box.max.z},
						};

						D3DRMBOX worldBox;
						{
							float x = corners[0].x * worldMatrix[0][0] + corners[0].y * worldMatrix[1][0] +
									  corners[0].z * worldMatrix[2][0] + worldMatrix[3][0];
							float y = corners[0].x * worldMatrix[0][1] + corners[0].y * worldMatrix[1][1] +
									  corners[0].z * worldMatrix[2][1] + worldMatrix[3][1];
							float z = corners[0].x * worldMatrix[0][2] + corners[0].y * worldMatrix[1][2] +
									  corners[0].z * worldMatrix[2][2] + worldMatrix[3][2];
							worldBox.min = {x, y, z};
							worldBox.max = {x, y, z};
						}

						for (int c = 1; c < 8; ++c) {
							float x = corners[c].x * worldMatrix[0][0] + corners[c].y * worldMatrix[1][0] +
									  corners[c].z * worldMatrix[2][0] + worldMatrix[3][0];
							float y = corners[c].x * worldMatrix[0][1] + corners[c].y * worldMatrix[1][1] +
									  corners[c].z * worldMatrix[2][1] + worldMatrix[3][1];
							float z = corners[c].x * worldMatrix[0][2] + corners[c].y * worldMatrix[1][2] +
									  corners[c].z * worldMatrix[2][2] + worldMatrix[3][2];

							if (x < worldBox.min.x) {
								worldBox.min.x = x;
							}
							if (y < worldBox.min.y) {
								worldBox.min.y = y;
							}
							if (z < worldBox.min.z) {
								worldBox.min.z = z;
							}
							if (x > worldBox.max.x) {
								worldBox.max.x = x;
							}
							if (y > worldBox.max.y) {
								worldBox.max.y = y;
							}
							if (z > worldBox.max.z) {
								worldBox.max.z = z;
							}
						}

						float distance = 0.0f;
						if (RayIntersectsBox(pickRay, worldBox, distance)) {
							auto* arr = new Direct3DRMFrameArray_SDL3GPUImpl();
							for (IDirect3DRMFrame* f : path) {
								arr->AddElement(f);
							}

							PickRecord rec;
							rec.visual = vis;
							rec.frameArray = arr;
							rec.desc.dist = distance;
							hits.push_back(rec);
						}
					}
					mesh->Release();
				}
				vis->Release();
			}
			visuals->Release();
		}
		path.pop_back(); // Pop after recursion
	};

	std::vector<IDirect3DRMFrame*> framePath;
	recurse(m_rootFrame, framePath);

	std::sort(hits.begin(), hits.end(), [](const PickRecord& a, const PickRecord& b) {
		return a.desc.dist < b.desc.dist;
	});

	*pickedArray = new Direct3DRMPickedArray_SDL3GPUImpl(hits.data(), hits.size());

	return D3DRM_OK;
}

void Direct3DRMViewport_SDL3GPUImpl::CloseDevice()
{
	FreeDeviceResources();
	m_device = nullptr;
}
