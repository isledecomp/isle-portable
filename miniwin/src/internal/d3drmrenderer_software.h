#pragma once

#include "d3drmrenderer.h"
#include "d3drmtexture_impl.h"
#include "ddraw_impl.h"

#include <SDL3/SDL.h>
#include <cstddef>
#include <vector>

DEFINE_GUID(SOFTWARE_GUID, 0x682656F3, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02);

struct TextureCache {
	Direct3DRMTextureImpl* texture;
	Uint8 version;
	SDL_Surface* cached;
};

struct MeshCache {
	const MeshGroup* meshGroup;
	int version;
	bool flat;
	std::vector<D3DRMVERTEX> vertices;
	std::vector<uint16_t> indices;
};

// Vertex after transform + lighting: rasterizer input.  Carrying the lit
// color through clipping (linear interpolation) avoids re-lighting split
// vertices and the normal renormalization that required.
struct SWLitVertex {
	D3DVECTOR position;
	TexCoord texCoord;
	SDL_Color color;
};

class Direct3DRMSoftwareRenderer : public Direct3DRMRenderer {
public:
	Direct3DRMSoftwareRenderer(DWORD width, DWORD height);
	~Direct3DRMSoftwareRenderer() override;
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

private:
	void ClearZBuffer();
	void DrawTriangleProjected(
		const SWLitVertex& v0,
		const SWLitVertex& v1,
		const SWLitVertex& v2,
		const Appearance& appearance
	);
	void DrawTriangleClipped(const SWLitVertex (&v)[3], const Appearance& appearance);
	void ProjectVertex(const D3DVECTOR& v, D3DRMVECTOR4D& p) const;
	SDL_Color ApplyLighting(const D3DVECTOR& position, const D3DVECTOR& normal, const Appearance& appearance);
	void AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture);
	void AddMeshDestroyCallback(Uint32 id, IDirect3DRMMesh* mesh);

	// Per-frame light data prepared once in PushLights so the per-vertex
	// lighting loop does no normalization.
	struct PreparedLight {
		D3DVECTOR vec; // normalized direction to light (directional) or position (positional)
		FColor color;
		bool positional;
	};

	SDL_Surface* m_renderedImage = nullptr;
	SDL_Palette* m_palette;
	SDL_Texture* m_uploadBuffer = nullptr;
	SDL_Renderer* m_renderer;
	const SDL_PixelFormatDetails* m_format;
	std::vector<PreparedLight> m_preparedLights;
	float m_ambientR = 0.0f;
	float m_ambientG = 0.0f;
	float m_ambientB = 0.0f;
	std::vector<TextureCache> m_textures;
	std::vector<MeshCache> m_meshs;
	D3DVALUE m_front;
	D3DVALUE m_back;
	Matrix3x3 m_normalMatrix;
	D3DRMMATRIX4D m_projection;
	std::vector<float> m_zBuffer;
	std::vector<D3DVECTOR> m_transformedPositions;
	std::vector<SDL_Color> m_vertexColors; // per-vertex lighting cache for the current draw
	std::vector<Uint8> m_vertexLit;
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
