#pragma once

#include "SDL3/SDL_log.h"
#include "d3drmrenderer.h"
#include "d3drmtexture_impl.h"
#include "ddraw_impl.h"

#include <SDL3/SDL.h>
#include <citro3d.h>
#include <vector>

DEFINE_GUID(Citro3D_GUID, 0x682656F3, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3D, 0x53);

struct C3DTextureCacheEntry {
	IDirect3DRMTexture* texture;
	Uint32 version;
	C3D_Tex c3dTex;
	uint16_t width;
	uint16_t height;
};

struct C3DMeshCacheEntry {
	const MeshGroup* meshGroup = nullptr;
	Uint32 version = 0;
	bool flat = false;

	C3D_AttrInfo attrInfo;
	C3D_BufInfo bufInfo;

	// CPU-side vertex data
	std::vector<D3DRMVERTEX> vertices;
	std::vector<D3DVECTOR> positions;
	std::vector<D3DVECTOR> normals;
	std::vector<TexCoord> texcoords; // Only if you have textures
	std::vector<uint16_t> indices;   // Indices for indexed drawing
};

class Citro3DRenderer : public Direct3DRMRenderer {
public:
	Citro3DRenderer(DWORD width, DWORD height);
	~Citro3DRenderer() override;

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
	C3DMeshCacheEntry UploadMesh(const MeshGroup& meshGroup);
	void AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture);
	void AddMeshDestroyCallback(Uint32 id, IDirect3DRMMesh* mesh);
	D3DRMMATRIX4D m_projection;
	C3D_Mtx m_projectionMatrix;
	SDL_Surface* m_renderedImage;
	C3D_RenderTarget* m_renderTarget;
	int m_projectionShaderUniformLocation;
	std::vector<C3DTextureCacheEntry> m_textures;
	std::vector<C3DMeshCacheEntry> m_meshs;
	ViewportTransform m_viewportTransform;

	// TODO: All these flags can likely be cleaned up
	bool m_flipVertFlag;
	bool m_outTiledFlag;
	bool m_rawCopyFlag;
	GX_TRANSFER_FORMAT m_transferInputFormatFlag;
	GX_TRANSFER_FORMAT m_transferOutputFormatFlag;
	GX_TRANSFER_SCALE m_transferScaleFlag;
	u32 m_transferFlags;
};

inline static void Citro3DRenderer_EnumDevice(LPD3DENUMDEVICESCALLBACK cb, void* ctx)
{
	GUID guid = Citro3D_GUID;
	char* deviceNameDup = SDL_strdup("Citro3D");
	char* deviceDescDup = SDL_strdup("Miniwin driver");
	D3DDEVICEDESC halDesc = {};
	halDesc.dcmColorModel = D3DCOLOR_RGB;
	halDesc.dwFlags = D3DDD_DEVICEZBUFFERBITDEPTH;
	halDesc.dwDeviceZBufferBitDepth = DDBD_24;
	halDesc.dwDeviceRenderBitDepth = DDBD_32;
	halDesc.dpcTriCaps.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE;
	halDesc.dpcTriCaps.dwShadeCaps = D3DPSHADECAPS_ALPHAFLATBLEND;
	halDesc.dpcTriCaps.dwTextureFilterCaps = D3DPTFILTERCAPS_LINEAR;
	D3DDEVICEDESC helDesc = {};

	cb(&guid, deviceNameDup, deviceDescDup, &halDesc, &helDesc, ctx);

	SDL_free(deviceDescDup);
	SDL_free(deviceNameDup);
}
