#pragma once

#include "d3drmrenderer.h"
#include "d3drmtexture_impl.h"
#include "ddraw_impl.h"
#include "ddpalette_impl.h"

#include <SDL3/SDL.h>
#include <vector>

#include <psp2/gxm.h>
#include <psp2/types.h>
#include <psp2/kernel/clib.h>

DEFINE_GUID(GXM_GUID, 0x682656F3, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0x58, 0x4D);

#define VITA_GXM_DISPLAY_BUFFER_COUNT 3

struct GXMTextureCacheEntry {
	IDirect3DRMTexture* texture;
	Uint32 version;
	SceGxmTexture gxmTexture;
	size_t textureSize;
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

typedef struct GXMDisplayData {
	void* address;
} GXMDisplayData;

struct SceneLightGXM {
	float color[4];
	float position[4];
	float direction[4];
};

typedef struct Vertex {
	float position[3];
    float normal[3];
    float texCoord[2];
} Vertex;


typedef struct GXMRendererContext {
	// context
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

	// shader patcher
	SceUID patcherBufferUid;
	void* patcherBuffer;

	SceUID patcherVertexUsseUid;
	size_t patcherVertexUsseOffset;
	void* patcherVertexUsse;
	
	SceUID patcherFragmentUsseUid;
	size_t patcherFragmentUsseOffset;
    void* patcherFragmentUsse;

	SceGxmShaderPatcher* shaderPatcher;
} GXMRendererContext;

typedef struct GXMRendererData {
	SceGxmContext* context;
	SceGxmShaderPatcher* shaderPatcher;
	SceClibMspace cdramPool;

	SceGxmRenderTarget* renderTarget;
	void* displayBuffers[VITA_GXM_DISPLAY_BUFFER_COUNT];
	SceUID displayBuffersUid[VITA_GXM_DISPLAY_BUFFER_COUNT];
	SceGxmColorSurface displayBuffersSurface[VITA_GXM_DISPLAY_BUFFER_COUNT];
	SceGxmSyncObject* displayBuffersSync[VITA_GXM_DISPLAY_BUFFER_COUNT];

	// depth buffer
	SceUID depthBufferUid;
	void* depthBufferData;
	SceUID stencilBufferUid;
	void* stencilBufferData;
	SceGxmDepthStencilSurface depthSurface;

	// main shader
	SceGxmShaderPatcherId mainVertexProgramId;
	SceGxmShaderPatcherId mainFragmentProgramId;
	SceGxmShaderPatcherId imageFragmentProgramId;
	SceGxmShaderPatcherId colorFragmentProgramId;

	SceGxmVertexProgram* mainVertexProgram;
	SceGxmFragmentProgram* opaqueFragmentProgram; // 3d with no transparency
	SceGxmFragmentProgram* transparentFragmentProgram; // 3d with transparency
	SceGxmFragmentProgram* imageFragmentProgram; // 2d images, no lighting
	SceGxmFragmentProgram* colorFragmentProgram; // 2d color, no lighting

	// main shader vertex uniforms
	const SceGxmProgramParameter* uModelViewMatrix;
	const SceGxmProgramParameter* uNormalMatrix;
	const SceGxmProgramParameter* uProjectionMatrix;

	// main shader fragment uniforms
	const SceGxmProgramParameter* uLights;
	const SceGxmProgramParameter* uLightCount;
	const SceGxmProgramParameter* uShininess;
	const SceGxmProgramParameter* uColor;
	const SceGxmProgramParameter* uUseTexture;

	const SceGxmProgramParameter* colorShader_uColor;

	// scene light data
	void* lightDataBuffer;

	void* quadMeshBuffer;
	uint16_t* quadIndices;
	Vertex* quadVertices;

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
	void GetDesc(D3DDEVICEDESC* halDesc, D3DDEVICEDESC* helDesc) override;
	const char* GetName() override;
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
	void AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture);
	void AddMeshDestroyCallback(Uint32 id, IDirect3DRMMesh* mesh);

	GXMMeshCacheEntry GXMUploadMesh(const MeshGroup& meshGroup);

	void StartScene();
	Vertex* GetQuadVertices();

	std::vector<GXMTextureCacheEntry> m_textures;
	std::vector<GXMMeshCacheEntry> m_meshes;
	D3DRMMATRIX4D m_projection;
	std::vector<SceneLight> m_lights;

	bool transparencyEnabled = false;
	bool sceneStarted = false;

	int backBufferIndex = 0;
	int frontBufferIndex = 1;

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
