#pragma once

#include "d3drmrenderer.h"
#include "d3drmtexture_impl.h"
#include "ddraw_impl.h"

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <SDL3/SDL.h>
#include <vector>

DEFINE_GUID(OpenGL1_GUID, 0x682656F3, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03);

struct GLTextureCacheEntry {
	IDirect3DRMTexture* texture;
	Uint32 version;
	GLuint glTextureId;
};

struct GLMeshCacheEntry {
	const MeshGroup* meshGroup;
	int version;
	bool flat;

	// non-VBO cache
	std::vector<D3DVECTOR> positions;
	std::vector<D3DVECTOR> normals;
	std::vector<TexCoord> texcoords;
	std::vector<uint16_t> indices;

	// VBO cache
	GLuint vboPositions;
	GLuint vboNormals;
	GLuint vboTexcoords;
	GLuint ibo;
};

class OpenGL1Renderer : public Direct3DRMRenderer {
public:
	static Direct3DRMRenderer* Create(DWORD width, DWORD height);
	OpenGL1Renderer(DWORD width, DWORD height, SDL_GLContext context);
	~OpenGL1Renderer() override;

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

	std::vector<GLTextureCacheEntry> m_textures;
	std::vector<GLMeshCacheEntry> m_meshs;
	D3DRMMATRIX4D m_projection;
	SDL_Surface* m_renderedImage;
	bool m_useVBOs;
	bool m_dirty = false;
	std::vector<SceneLight> m_lights;
	SDL_GLContext m_context;
	ViewportTransform m_viewportTransform;
};

inline static void OpenGL1Renderer_EnumDevice(LPD3DENUMDEVICESCALLBACK cb, void* ctx)
{
	Direct3DRMRenderer* device = OpenGL1Renderer::Create(640, 480);
	if (device) {
		EnumDevice(cb, ctx, device, OpenGL1_GUID);
		delete device;
	}
}
