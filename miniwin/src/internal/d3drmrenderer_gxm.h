#pragma once

#include "../d3drm/backends/gxm/gxm_context.h"
#include "d3drmrenderer.h"
#include "d3drmtexture_impl.h"
#include "ddpalette_impl.h"
#include "ddraw_impl.h"

#include <mortar/mortar.h>
#include <psp2/gxm.h>
#include <psp2/kernel/clib.h>
#include <psp2/types.h>
#include <vector>

DEFINE_GUID(GXM_GUID, 0x682656F3, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0x58, 0x4D);

#define GXM_VERTEX_BUFFER_COUNT 2
#define GXM_FRAGMENT_BUFFER_COUNT 3

#define GXM_WITH_RAZOR DEBUG

struct GXMTextureCacheEntry {
	IDirect3DRMTexture* texture;
	uint32_t version;
	SceGxmTexture gxmTexture;
	SceGxmNotification* notification; // latest frame it was used in
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

struct SceneLightGXM {
	float color[4];
	float vec[4];
	float isDirectional;
	float _align;
};

struct GXMSceneLightUniform {
	SceneLightGXM lights[2];
	float ambientLight[3];
};

class GXMRenderer : public Direct3DRMRenderer {
public:
	static Direct3DRMRenderer* Create(DWORD width, DWORD height, DWORD msaaSamples);
	GXMRenderer(DWORD width, DWORD height, SceGxmMultisampleMode msaaMode);
	~GXMRenderer() override;

	void PushLights(const SceneLight* lightsArray, size_t count) override;
	void SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back) override;
	void SetFrustumPlanes(const Plane* frustumPlanes) override;
	uint32_t GetTextureId(IDirect3DRMTexture* texture, bool isUi, float scaleX, float scaleY) override;
	uint32_t GetMeshId(IDirect3DRMMesh* mesh, const MeshGroup* meshGroup) override;
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

	void DeferredDelete(int index);

private:
	void AddTextureDestroyCallback(uint32_t id, IDirect3DRMTexture* texture);
	void AddMeshDestroyCallback(uint32_t id, IDirect3DRMMesh* mesh);

	GXMMeshCacheEntry GXMUploadMesh(const MeshGroup& meshGroup);

	void StartScene();
	const SceGxmTexture* UseTexture(GXMTextureCacheEntry& texture);

	inline GXMVertex2D* QuadVerticesBuffer()
	{
		GXMVertex2D* verts = &this->quadVertices[this->currentVertexBufferIndex][this->quadsUsed * 4];
		this->quadsUsed += 1;
		if (this->quadsUsed >= 50) {
			MORTAR_Log("QuadVerticesBuffer overflow");
			this->quadsUsed = 0; // declare bankruptcy
		}
		return verts;
	}

	inline GXMSceneLightUniform* LightsBuffer() { return this->lights[this->currentFragmentBufferIndex]; }

	std::vector<GXMTextureCacheEntry> m_textures;
	std::vector<GXMMeshCacheEntry> m_meshes;
	D3DRMMATRIX4D m_projection;
	std::vector<SceneLight> m_lights;
	std::vector<SceGxmTexture> m_textures_delete[GXM_FRAGMENT_BUFFER_COUNT];
	std::vector<void*> m_buffers_delete[GXM_FRAGMENT_BUFFER_COUNT];

	bool transparencyEnabled = false;

	// shader
	SceGxmShaderPatcherId mainVertexProgramId;
	SceGxmShaderPatcherId mainColorFragmentProgramId;
	SceGxmShaderPatcherId mainTextureFragmentProgramId;

	SceGxmVertexProgram* mainVertexProgram;
	SceGxmFragmentProgram* opaqueColorFragmentProgram;
	SceGxmFragmentProgram* blendedColorFragmentProgram;
	SceGxmFragmentProgram* opaqueTextureFragmentProgram;
	SceGxmFragmentProgram* blendedTextureFragmentProgram;

	// main shader vertex uniforms
	const SceGxmProgramParameter* uModelViewMatrix;
	const SceGxmProgramParameter* uNormalMatrix;
	const SceGxmProgramParameter* uProjectionMatrix;

	// main shader fragment uniforms
	const SceGxmProgramParameter* uShininess;
	const SceGxmProgramParameter* uColor;
	const SceGxmProgramParameter* uLights;
	const SceGxmProgramParameter* uAmbientLight;

	// uniforms / quad meshes
	GXMSceneLightUniform* lights[GXM_FRAGMENT_BUFFER_COUNT];
	GXMVertex2D* quadVertices[GXM_VERTEX_BUFFER_COUNT];
	uint16_t* quadIndices;
	int quadsUsed = 0;
	bool cleared = false;

	SceGxmNotification vertexNotifications[GXM_VERTEX_BUFFER_COUNT];
	SceGxmNotification fragmentNotifications[GXM_FRAGMENT_BUFFER_COUNT];
	int currentFragmentBufferIndex = 0;
	int currentVertexBufferIndex = 0;

#ifdef GXM_WITH_RAZOR
	bool last_dpad_up;
	bool last_dpad_down;
	bool last_dpad_left;
	bool last_dpad_right;
#endif

	bool m_initialized = false;
};

int gxm_library_init();

inline static void GXMRenderer_EnumDevice(LPD3DENUMDEVICESCALLBACK cb, void* ctx)
{
	D3DDEVICEDESC halDesc = {};
	halDesc.dcmColorModel = D3DCOLORMODEL::RGB;
	halDesc.dwFlags = D3DDD_DEVICEZBUFFERBITDEPTH;
	halDesc.dwDeviceZBufferBitDepth = DDBD_16;
	halDesc.dwDeviceZBufferBitDepth |= DDBD_32;
	halDesc.dpcTriCaps.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE;
	halDesc.dpcTriCaps.dwShadeCaps = D3DPSHADECAPS_ALPHAFLATBLEND;
	halDesc.dpcTriCaps.dwTextureFilterCaps = D3DPTFILTERCAPS_LINEAR;

	D3DDEVICEDESC helDesc = {};
	helDesc.dwDeviceRenderBitDepth = DDBD_32;

	int ret = gxm_library_init();
	if (ret < 0) {
		MORTAR_Log("gxm_library_init failed: %08x", ret);
		return;
	}

	EnumDevice(cb, ctx, "GXM HAL", &halDesc, &helDesc, GXM_GUID);
}
