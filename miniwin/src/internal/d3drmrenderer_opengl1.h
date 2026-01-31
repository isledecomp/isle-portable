#pragma once
#include "../d3drm/backends/opengl1/actual.h"
#include "d3drmrenderer.h"
#include "d3drmtexture_impl.h"
#include "ddraw_impl.h"

#include <mortar/mortar.h>
#include <vector>

DEFINE_GUID(OpenGL1_GUID, 0x682656F3, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03);

class OpenGL1Renderer : public Direct3DRMRenderer {
public:
	static Direct3DRMRenderer* Create(DWORD width, DWORD height, DWORD msaaSamples);
	OpenGL1Renderer(DWORD width, DWORD height, MORTAR_GLContext context);
	~OpenGL1Renderer() override;

	void PushLights(const SceneLight* lightsArray, size_t count) override;
	void SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back) override;
	void SetFrustumPlanes(const Plane* frustumPlanes) override;
	uint32_t GetTextureId(IDirect3DRMTexture* texture, bool isUI, float scaleX, float scaleY) override;
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

private:
	void AddTextureDestroyCallback(uint32_t id, IDirect3DRMTexture* texture);
	void AddMeshDestroyCallback(uint32_t id, IDirect3DRMMesh* mesh);

	std::vector<GLTextureCacheEntry> m_textures;
	std::vector<GLMeshCacheEntry> m_meshs;
	D3DRMMATRIX4D m_projection;
	MORTAR_Surface* m_renderedImage;
	bool m_useVBOs;
	bool m_useNPOT;
	bool m_dirty = false;
	std::vector<SceneLight> m_lights;
	MORTAR_GLContext m_context;
	ViewportTransform m_viewportTransform;
};

inline static void OpenGL1Renderer_EnumDevice(const IDirect3DMiniwin* d3d, LPD3DENUMDEVICESCALLBACK cb, void* ctx)
{
	Direct3DRMRenderer* device = OpenGL1Renderer::Create(640, 480, d3d->GetMSAASamples());
	if (!device) {
		return;
	}
	delete device;

	D3DDEVICEDESC halDesc = {};
	halDesc.dcmColorModel = D3DCOLORMODEL::RGB;
	halDesc.dwFlags = D3DDD_DEVICEZBUFFERBITDEPTH;
	halDesc.dwDeviceZBufferBitDepth = DDBD_24;
	halDesc.dwDeviceRenderBitDepth = DDBD_32;
	halDesc.dpcTriCaps.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE;
	halDesc.dpcTriCaps.dwShadeCaps = D3DPSHADECAPS_ALPHAFLATBLEND;
	halDesc.dpcTriCaps.dwTextureFilterCaps = D3DPTFILTERCAPS_LINEAR;

	D3DDEVICEDESC helDesc = {};

	EnumDevice(cb, ctx, "OpenGL 1.1 HAL", &halDesc, &helDesc, OpenGL1_GUID);
}
