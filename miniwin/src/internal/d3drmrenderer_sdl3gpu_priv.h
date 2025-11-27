#pragma once

#include "d3drmrenderer.h"
#include "d3drmrenderer_sdl3gpu.h"
#include "d3drmtexture_impl.h"
#include "ddraw_impl.h"
#include "ddsurface_impl.h"

#include <SDL3/SDL.h>
#include <vector>

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
	uint32_t version;
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
	uint32_t GetTextureId(IDirect3DRMTexture* texture, bool isUI, float scaleX, float scaleY) override;
	uint32_t GetMeshId(IDirect3DRMMesh* mesh, const MeshGroup* meshGroup) override;
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
	void Draw2DImage(uint32_t textureId, const MORTAR_Rect& srcRect, const MORTAR_Rect& dstRect, FColor color) override;
	void Download(MORTAR_Surface* target) override;
	void SetDither(bool dither) override;

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
