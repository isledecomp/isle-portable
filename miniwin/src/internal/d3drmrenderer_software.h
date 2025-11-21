#pragma once

#include "d3drmrenderer.h"
#include "d3drmtexture_impl.h"
#include "ddraw_impl.h"

#include <cstddef>
#include <mortar/mortar.h>
#include <vector>

DEFINE_GUID(SOFTWARE_GUID, 0x682656F3, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02);

struct TextureCache {
	Direct3DRMTextureImpl* texture;
	uint8_t version;
	MORTAR_Surface* cached;
};

struct MeshCache {
	const MeshGroup* meshGroup;
	int version;
	bool flat;
	std::vector<D3DRMVERTEX> vertices;
	std::vector<uint16_t> indices;
};

class Direct3DRMSoftwareRenderer : public Direct3DRMRenderer {
public:
	Direct3DRMSoftwareRenderer(DWORD width, DWORD height);
	~Direct3DRMSoftwareRenderer() override;
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
	void ClearZBuffer();
	void DrawTriangleProjected(
		const D3DRMVERTEX& v0,
		const D3DRMVERTEX& v1,
		const D3DRMVERTEX& v2,
		const Appearance& appearance
	);
	void DrawTriangleClipped(const D3DRMVERTEX (&v)[3], const Appearance& appearance);
	void ProjectVertex(const D3DVECTOR& v, D3DRMVECTOR4D& p) const;
	uint32_t BlendPixel(uint8_t* pixelAddr, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
	MORTAR_Color ApplyLighting(const D3DVECTOR& position, const D3DVECTOR& normal, const Appearance& appearance);
	void AddTextureDestroyCallback(uint32_t id, IDirect3DRMTexture* texture);
	void AddMeshDestroyCallback(uint32_t id, IDirect3DRMMesh* mesh);

	MORTAR_Surface* m_renderedImage = nullptr;
	MORTAR_Palette* m_palette = nullptr;
	MORTAR_Texture* m_uploadBuffer = nullptr;
	MORTAR_Renderer* m_renderer;
	const MORTAR_PixelFormatDetails* m_format;
	int m_bytesPerPixel;
	std::vector<SceneLight> m_lights;
	std::vector<TextureCache> m_textures;
	std::vector<MeshCache> m_meshs;
	D3DVALUE m_front;
	D3DVALUE m_back;
	Matrix3x3 m_normalMatrix;
	D3DRMMATRIX4D m_projection;
	std::vector<float> m_zBuffer;
	std::vector<D3DRMVERTEX> m_transformedVerts;
	Plane m_frustumPlanes[6];
};

inline static void Direct3DRMSoftware_EnumDevice(LPD3DENUMDEVICESCALLBACK cb, void* ctx)
{
	D3DDEVICEDESC halDesc = {};

	D3DDEVICEDESC helDesc = {};
	helDesc.dcmColorModel = D3DCOLOR_RGB;
	helDesc.dwFlags = D3DDD_DEVICEZBUFFERBITDEPTH;
	helDesc.dwDeviceZBufferBitDepth = DDBD_32;
	helDesc.dwDeviceRenderBitDepth = DDBD_32;
	helDesc.dpcTriCaps.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE;
	helDesc.dpcTriCaps.dwShadeCaps = D3DPSHADECAPS_ALPHAFLATBLEND;
	helDesc.dpcTriCaps.dwTextureFilterCaps = D3DPTFILTERCAPS_LINEAR;

	EnumDevice(cb, ctx, "Miniwin Emulation", &halDesc, &helDesc, SOFTWARE_GUID);
}
