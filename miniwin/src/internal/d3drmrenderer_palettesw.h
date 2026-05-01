#pragma once

#include "d3drmrenderer.h"
#include "d3drmtexture_impl.h"
#include "ddraw_impl.h"

#include <SDL3/SDL.h>
#include <cstddef>
#include <vector>

DEFINE_GUID(PALETTE_SW_GUID, 0x682656F3, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07);

struct PaletteTextureCache {
	Direct3DRMTextureImpl* texture;
	Uint8 version;
	SDL_Surface* cached;
};

struct PaletteMeshCache {
	const MeshGroup* meshGroup;
	int version;
	bool flat;
	std::vector<D3DRMVERTEX> vertices;
	std::vector<uint16_t> indices;
};

class Direct3DRMPaletteSWRenderer : public Direct3DRMRenderer {
public:
	Direct3DRMPaletteSWRenderer(DWORD width, DWORD height);
	~Direct3DRMPaletteSWRenderer() override;
	void PushLights(const SceneLight* vertices, size_t count) override;
	Uint32 GetTextureId(IDirect3DRMTexture* texture, bool isUI, float scaleX, float scaleY) override;
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
	void Draw2DImage(Uint32 textureId, const SDL_Rect& srcRect, const SDL_Rect& dstRect, FColor color) override;
	void Download(SDL_Surface* target) override;
	void SetDither(bool dither) override;
	void SetPalette(SDL_Palette* palette) override;
	bool UsesPalettedSurfaces() const override { return true; }

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
	Uint8 ApplyLighting(const D3DVECTOR& position, const D3DVECTOR& normal, const Appearance& appearance, Uint8 texel);
	void BuildLightingLUT();
	void BuildBlendLUT();
	void AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture);
	void AddMeshDestroyCallback(Uint32 id, IDirect3DRMMesh* mesh);

	SDL_Surface* m_renderedImage = nullptr;
	SDL_Palette* m_palette = nullptr;
	SDL_Palette* m_flipPalette = nullptr; // Palette snapshot taken at Flip time (the correct one)
	bool m_flipPaletteDirty = false;
	std::vector<SceneLight> m_lights;
	std::vector<PaletteTextureCache> m_textures;
	std::vector<PaletteMeshCache> m_meshes;
	D3DVALUE m_front;
	D3DVALUE m_back;
	Matrix3x3 m_normalMatrix;
	D3DRMMATRIX4D m_projection;
	std::vector<float> m_zBuffer;
	std::vector<D3DRMVERTEX> m_transformedVerts;
	Plane m_frustumPlanes[6];

	// Lighting LUT: for each of 256 palette entries x 32 brightness levels,
	// store the best-matching palette index.
	// Usage: m_lightLUT[paletteIndex * 32 + brightnessLevel]
	static constexpr int LIGHT_LEVELS = 32;
	Uint8 m_lightLUT[256 * LIGHT_LEVELS];

	// Blend LUT: for any two palette indices, the pre-computed 50/50 blend
	// result mapped to the nearest palette colour.
	// Usage: m_blendLUT[srcIndex * 256 + dstIndex]
	Uint8 m_blendLUT[256 * 256];

	bool m_lightLUTDirty = true;
	bool m_transparencyEnabled = false;
};

inline static void Direct3DRMPaletteSW_EnumDevice(LPD3DENUMDEVICESCALLBACK cb, void* ctx)
{
	D3DDEVICEDESC halDesc = {};

	D3DDEVICEDESC helDesc = {};
	helDesc.dcmColorModel = D3DCOLOR_RGB;
	helDesc.dwFlags = D3DDD_DEVICEZBUFFERBITDEPTH;
	helDesc.dwDeviceZBufferBitDepth = DDBD_16;
	helDesc.dwDeviceRenderBitDepth = DDBD_8;
	helDesc.dpcTriCaps.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE;
	helDesc.dpcTriCaps.dwShadeCaps = D3DPSHADECAPS_ALPHAFLATBLEND;
	helDesc.dpcTriCaps.dwTextureFilterCaps = D3DPTFILTERCAPS_LINEAR;

	EnumDevice(cb, ctx, "Miniwin Paletted Software", &halDesc, &helDesc, PALETTE_SW_GUID);
}
