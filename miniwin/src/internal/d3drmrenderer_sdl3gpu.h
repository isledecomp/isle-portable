#pragma once

#include "d3drmrenderer.h"
#include "d3drmtexture_impl.h"
#include "ddraw_impl.h"
#include "ddsurface_impl.h"

#include <SDL3/SDL.h>
#include <vector>

DEFINE_GUID(SDL3_GPU_GUID, 0x682656F3, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01);

typedef struct {
	D3DRMMATRIX4D projection;
	D3DRMMATRIX4D worldViewMatrix;
	D3DRMMATRIX4D normalMatrix;
} ViewportUniforms;
static_assert(sizeof(ViewportUniforms) % 16 == 0);
static_assert(sizeof(ViewportUniforms) == 192);

struct FragmentShadingData {
	SceneLight lights[3];
	int lightCount;
	float shininess;
	SDL_Color color;
	int useTexture;
};
static_assert(sizeof(FragmentShadingData) % 16 == 0);
static_assert(sizeof(FragmentShadingData) == 160);

struct SDL3TextureCache {
	Direct3DRMTextureImpl* texture;
	Uint32 version;
	SDL_GPUTexture* gpuTexture;
};

struct SDL3MeshCache {
	const MeshGroup* meshGroup;
	int version;
	SDL_GPUBuffer* vertexBuffer;
	SDL_GPUBuffer* indexBuffer;
	size_t indexCount;
};

class Direct3DRMSDL3GPURenderer : public Direct3DRMRenderer {
public:
	static Direct3DRMRenderer* Create(DWORD width, DWORD height);
	~Direct3DRMSDL3GPURenderer() override;
	void PushLights(const SceneLight* vertices, size_t count) override;
	Uint32 GetTextureId(IDirect3DRMTexture* texture, bool isUi) override;
	Uint32 GetMeshId(IDirect3DRMMesh* mesh, const MeshGroup* meshGroup) override;
	void SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back) override;
	void SetFrustumPlanes(const Plane* frustumPlanes) override;
	HRESULT BeginFrame() override;
	void EnableTransparency() override;
	void SubmitDraw(
		DWORD meshId,
		const D3DRMMATRIX4D& modelViewMatrix,
		const D3DRMMATRIX4D& worldMatrix,
		const D3DRMMATRIX4D& viewMatrix,
		const Matrix3x3& normalMatrix,
		const Appearance& appearance
	) override;
	HRESULT FinalizeFrame() override;
	void Resize(int width, int height, const ViewportTransform& viewportTransform) override;
	void Clear(float r, float g, float b) override;
	void Flip() override;
	void Draw2DImage(Uint32 textureId, const SDL_Rect& srcRect, const SDL_Rect& dstRect) override;
	void Download(SDL_Surface* target) override;

private:
	Direct3DRMSDL3GPURenderer(
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
	);
	void StartRenderPass(float r, float g, float b, bool clear);
	void WaitForPendingUpload();
	void AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture);
	SDL_GPUTransferBuffer* GetUploadBuffer(size_t size);
	SDL_GPUTexture* CreateTextureFromSurface(SDL_Surface* surface);
	void AddMeshDestroyCallback(Uint32 id, IDirect3DRMMesh* mesh);
	SDL3MeshCache UploadMesh(const MeshGroup& meshGroup);

	MeshGroup m_uiMesh;
	SDL3MeshCache m_uiMeshCache;
	D3DVALUE m_front;
	D3DVALUE m_back;
	ViewportUniforms m_uniforms;
	FragmentShadingData m_fragmentShadingData;
	D3DDEVICEDESC m_desc;
	D3DRMMATRIX4D m_projection;
	std::vector<SDL3TextureCache> m_textures;
	std::vector<SDL3MeshCache> m_meshs;
	SDL_GPUDevice* m_device;
	SDL_GPUGraphicsPipeline* m_opaquePipeline;
	SDL_GPUGraphicsPipeline* m_transparentPipeline;
	SDL_GPUGraphicsPipeline* m_uiPipeline;
	SDL_GPUTexture* m_transferTexture = nullptr;
	SDL_GPUTexture* m_depthTexture = nullptr;
	SDL_GPUTexture* m_dummyTexture;
	int m_uploadBufferSize;
	SDL_GPUTransferBuffer* m_uploadBuffer;
	SDL_GPUTransferBuffer* m_downloadBuffer = nullptr;
	SDL_GPUBuffer* m_vertexBuffer = nullptr;
	SDL_GPUSampler* m_sampler;
	SDL_GPUSampler* m_uiSampler;
	SDL_GPUCommandBuffer* m_cmdbuf = nullptr;
	SDL_GPURenderPass* m_renderPass = nullptr;
	SDL_GPUFence* m_uploadFence = nullptr;
};

inline static void Direct3DRMSDL3GPU_EnumDevice(LPD3DENUMDEVICESCALLBACK cb, void* ctx)
{
	Direct3DRMRenderer* device = Direct3DRMSDL3GPURenderer::Create(640, 480);
	if (!device) {
		return;
	}
	delete device;

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
