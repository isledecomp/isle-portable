#pragma once

#include "d3drmrenderer.h"
#include "d3drmtexture_impl.h"
#include "ddraw_impl.h"

#include <SDL3/SDL.h>
#include <vector>

#include <psp2/gxm.h>
#include <psp2/types.h>
#include <psp2/kernel/clib.h>

DEFINE_GUID(GXM_GUID, 0x682656F3, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0x58, 0x4D);

#define VITA_GXM_DISPLAY_BUFFER_COUNT 2

struct GXMTextureCacheEntry {
	IDirect3DRMTexture* texture;
	Uint32 version;
	SceGxmTexture gxmTexture;
};

struct GXMMeshCacheEntry {
	const MeshGroup* meshGroup;
	int version;
	bool flat;

	void* meshData;
	void* vertexBuffer;
	void* indexBuffer;
	uint16_t indexCount;
};

typedef struct GXMRendererInit {
	SceGxmContext* context;
	SceGxmShaderPatcher* shaderPatcher;
	SceClibMspace gpuPool;
} GXMRendererInit;

typedef struct GXMRendererData {
	SceUID vdmRingBufferUid;
	SceUID vertexRingBufferUid;
	SceUID fragmentRingBufferUid;
	SceUID fragmentUsseRingBufferUid;
	size_t fragmentUsseRingBufferOffset;

	void* vdmRingBuffer;
	void* vertexRingBuffer;
	void* fragmentRingBuffer;
	void* fragmentUsseRingBuffer;
	
	SceClibMspace cdramPool;

	void* contextHostMem;
	SceGxmContext* context;

	SceGxmRenderTarget* renderTarget;
	SceUID renderBufferUid;
	void* renderBuffer;
	SceGxmColorSurface renderSurface;
	SceGxmSyncObject* renderBufferSync;

	SceUID depthBufferUid;
	void* depthBufferData;
	SceUID stencilBufferUid;
	void* stencilBufferData;
	SceGxmDepthStencilSurface depthSurface;

	SceUID patcherBufferUid;
	void* patcherBuffer;

	SceUID patcherVertexUsseUid;
	size_t patcherVertexUsseOffset;
	void* patcherVertexUsse;
	
	SceUID patcherFragmentUsseUid;
	size_t patcherFragmentUsseOffset;
    void* patcherFragmentUsse;

	SceGxmShaderPatcher* shaderPatcher;

	SceGxmShaderPatcherId clearVertexProgramId;
	SceGxmVertexProgram* clearVertexProgram;
	SceGxmShaderPatcherId clearFragmentProgramId;
	SceGxmFragmentProgram* clearFragmentProgram;

	SceGxmShaderPatcherId mainVertexProgramId;
	SceGxmShaderPatcherId mainFragmentProgramId;
	SceGxmVertexProgram* mainVertexProgram;
	SceGxmFragmentProgram* opaqueFragmentProgram;
	SceGxmFragmentProgram* transparentFragmentProgram;

	const SceGxmProgramParameter* uModelViewMatrixParam;
	const SceGxmProgramParameter* uNormalMatrixParam;
	const SceGxmProgramParameter* uProjectionMatrixParam;

	const SceGxmProgramParameter* uLights;
	const SceGxmProgramParameter* uLightCount;
	const SceGxmProgramParameter* uShininess;
	const SceGxmProgramParameter* uColor;
	const SceGxmProgramParameter* uUseTexture;

	void* clearMeshBuffer;
	float* clearVerticies;
	uint16_t* clearIndicies;

	void* lightDataBuffer;
} GXMRendererData;

class GXMRenderer : public Direct3DRMRenderer {
public:
	static Direct3DRMRenderer* Create(DWORD width, DWORD height);
	GXMRenderer(
		DWORD width,
		DWORD height,
		GXMRendererData data
	);
	GXMRenderer(int forEnum) {};
	~GXMRenderer() override;

	void PushLights(const SceneLight* lightsArray, size_t count) override;
	void SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back) override;
	void SetFrustumPlanes(const Plane* frustumPlanes) override;
	Uint32 GetTextureId(IDirect3DRMTexture* texture) override;
	Uint32 GetMeshId(IDirect3DRMMesh* mesh, const MeshGroup* meshGroup) override;
	DWORD GetWidth() override;
	DWORD GetHeight() override;
	void GetDesc(D3DDEVICEDESC* halDesc, D3DDEVICEDESC* helDesc) override;
	const char* GetName() override;
	HRESULT BeginFrame() override;
	void EnableTransparency() override;
	void SubmitDraw(
		DWORD meshId,
		const D3DRMMATRIX4D& modelViewMatrix,
		const Matrix3x3& normalMatrix,
		const Appearance& appearance
	) override;
	HRESULT FinalizeFrame() override;

private:
	void AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture);
	void AddMeshDestroyCallback(Uint32 id, IDirect3DRMMesh* mesh);

	void* AllocateGpu(size_t size, size_t align = 4);
	void FreeGpu(void* ptr);

	GXMMeshCacheEntry GXMUploadMesh(const MeshGroup& meshGroup);

	std::vector<GXMTextureCacheEntry> m_textures;
	std::vector<GXMMeshCacheEntry> m_meshes;
	D3DRMMATRIX4D m_projection;
	DWORD m_width, m_height;
	std::vector<SceneLight> m_lights;

	GXMRendererData m_data;
	bool m_initialized = false;
};

inline static void GXMRenderer_EnumDevice(LPD3DENUMDEVICESCALLBACK cb, void* ctx)
{
	Direct3DRMRenderer* device = new GXMRenderer(1);
	if (device) {
		EnumDevice(cb, ctx, device, GXM_GUID);
		delete device;
	}
}
