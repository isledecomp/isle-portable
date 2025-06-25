#pragma once

#include "SDL3/SDL_log.h"
#include "d3drmrenderer.h"
#include "d3drmtexture_impl.h"
#include "ddraw_impl.h"

#include <citro3d.h>
#include <SDL3/SDL.h>
#include <c3d/texture.h>
#include <vector>

DEFINE_GUID(Citro3D_GUID, 0x682656F3, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07);

struct C3DTextureCacheEntry {
	IDirect3DRMTexture* texture;
	Uint32 version;
	C3D_Tex* c3dTex;
};

class Citro3DRenderer : public Direct3DRMRenderer {
public:
	static Direct3DRMRenderer* Create(DWORD width, DWORD height);

	// constructor parameters not finalized
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
	void AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture);
	D3DRMMATRIX4D m_projection;
	C3D_Mtx m_projectionMatrix;
	SDL_Surface* m_renderedImage;
	C3D_RenderTarget* m_renderTarget;
	int m_projectionShaderUniformLocation;
	std::vector<C3DTextureCacheEntry> m_textures;

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
	SDL_Log("Hello, enuming device");
	Direct3DRMRenderer* device = Citro3DRenderer::Create(400, 240);
	if (device) {
		EnumDevice(cb, ctx, device, Citro3D_GUID);
		delete device;
	}
}
