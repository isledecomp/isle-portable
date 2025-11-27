#pragma once

#include "d3drmrenderer.h"
#include "ddraw_impl.h"

#include <citro3d.h>
#include <mortar/mortar.h>
#include <vector>

DEFINE_GUID(Citro3D_GUID, 0x682656F3, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3D, 0x53);

struct C3DTextureCacheEntry {
	IDirect3DRMTexture* texture;
	uint32_t version;
	C3D_Tex c3dTex;
	uint16_t width;
	uint16_t height;
};

struct C3DMeshCacheEntry {
	const MeshGroup* meshGroup = nullptr;
	int version = 0;
	void* vbo = nullptr;
	int vertexCount = 0;
};

class Citro3DRenderer : public Direct3DRMRenderer {
public:
	Citro3DRenderer(DWORD width, DWORD height);
	~Citro3DRenderer() override;

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
	void StartFrame();

	D3DRMMATRIX4D m_projection;
	MORTAR_Surface* m_renderedImage;
	C3D_RenderTarget* m_renderTarget;
	std::vector<C3DTextureCacheEntry> m_textures;
	std::vector<C3DMeshCacheEntry> m_meshs;
	ViewportTransform m_viewportTransform;
	std::vector<SceneLight> m_lights;
};

inline static void Citro3DRenderer_EnumDevice(LPD3DENUMDEVICESCALLBACK cb, void* ctx)
{
	D3DDEVICEDESC halDesc = {};
	halDesc.dcmColorModel = D3DCOLOR_RGB;
	halDesc.dwFlags = D3DDD_DEVICEZBUFFERBITDEPTH;
	halDesc.dwDeviceZBufferBitDepth = DDBD_24;
	halDesc.dwDeviceRenderBitDepth = DDBD_32;
	halDesc.dpcTriCaps.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE;
	halDesc.dpcTriCaps.dwShadeCaps = D3DPSHADECAPS_ALPHAFLATBLEND;
	halDesc.dpcTriCaps.dwTextureFilterCaps = D3DPTFILTERCAPS_LINEAR;

	D3DDEVICEDESC helDesc = {};

	EnumDevice(cb, ctx, "Citro3D", &halDesc, &helDesc, Citro3D_GUID);
}
