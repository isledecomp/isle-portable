#pragma once

#include "d3drmrenderer.h"
#include "d3drmtexture_impl.h"
#include "ddraw_impl.h"

extern "C"
{
#include <glide.h>
}

#ifdef GLIDE3
// Glide 3 uses custom vertex layout - define our own struct
struct GlideVertex {
	float x, y;       // screen coords
	float ooz;        // 65535/Z (for Z-buffering)
	float oow;        // 1/w (for perspective correction)
	float r, g, b, a; // color (0-255)
	float sow, tow;   // texture coords (s/w, t/w)
};
#define GR_WDEPTHVALUE_FARTHEST 0xFFFF
#else
typedef GrVertex GlideVertex;
#endif

#include <vector>

// clang-format off
DEFINE_GUID(GLIDE_GUID, 0x682656F3, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08);
// clang-format on

struct GlideTextureEntry {
	IDirect3DRMTexture* texture;
	Uint32 version;
	GrTexInfo info;
	FxU32 startAddress;
	int texW; // actual power-of-2 width uploaded to Glide
	int texH; // actual power-of-2 height uploaded to Glide
};

struct GlideMeshEntry {
	const MeshGroup* meshGroup;
	Uint32 version;
	bool flat;
	// Cached flattened geometry (only populated for flat-shaded meshes)
	std::vector<D3DRMVERTEX> flatVertices;
	std::vector<uint16_t> flatIndices;
};

class Direct3DRMGlideRenderer : public Direct3DRMRenderer {
public:
	Direct3DRMGlideRenderer(int width, int height);
	~Direct3DRMGlideRenderer() override;

	void PushLights(const SceneLight* lights, size_t count) override;
	void SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back) override;
	void SetFrustumPlanes(const Plane* frustumPlanes) override;
	Uint32 GetTextureId(IDirect3DRMTexture* texture, bool isUI = false, float scaleX = 0, float scaleY = 0) override;
	Uint32 GetMeshId(IDirect3DRMMesh* mesh, const MeshGroup* meshGroup) override;
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
	std::vector<GlideTextureEntry> m_textureCache;
	std::vector<GlideMeshEntry> m_meshCache;
	std::vector<SceneLight> m_lights;
	D3DRMMATRIX4D m_projection;
	D3DVALUE m_frontClip;
	D3DVALUE m_backClip;
	Plane m_frustumPlanes[6];
	std::vector<D3DRMVERTEX> m_transformedVertices;
	std::vector<SDL_Color> m_litColors;
	bool m_transparencyEnabled;
	FxU32 m_nextTextureAddress;
	SDL_Palette* m_palette = nullptr;
	bool m_paletteUploaded = false;
#ifdef GLIDE3
	FxU32 m_glideContext = 0;
#endif
};

inline static void Direct3DRMGlide_EnumDevice(LPD3DENUMDEVICESCALLBACK cb, void* ctx)
{
	D3DDEVICEDESC halDesc = {};
	D3DDEVICEDESC helDesc = {};
	halDesc.dcmColorModel = D3DCOLOR_RGB;
	halDesc.dwFlags = D3DDD_DEVICEZBUFFERBITDEPTH;
	halDesc.dwDeviceZBufferBitDepth = DDBD_16;
	halDesc.dwDeviceRenderBitDepth = DDBD_16;
	halDesc.dpcTriCaps.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE;
	halDesc.dpcTriCaps.dwShadeCaps = D3DPSHADECAPS_ALPHAFLATBLEND;
	halDesc.dpcTriCaps.dwTextureFilterCaps = D3DPTFILTERCAPS_LINEAR;
	EnumDevice(cb, ctx, "3dfx Glide", &halDesc, &helDesc, GLIDE_GUID);
}
