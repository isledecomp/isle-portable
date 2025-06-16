#pragma once

#include "d3drmrenderer.h"
#include "d3drmtexture_impl.h"
#include "ddraw_impl.h"

#include <GLES2/gl2.h>
#include <SDL3/SDL.h>
#include <vector>

DEFINE_GUID(OpenGLES2_GUID, 0x682656F3, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04);

struct GLES2TextureCacheEntry {
	IDirect3DRMTexture* texture;
	Uint32 version;
	GLuint glTextureId;
};

struct GLES2MeshCacheEntry {
	const MeshGroup* meshGroup;
	int version;
	bool flat;

	std::vector<uint16_t> indices;
	GLuint vboPositions;
	GLuint vboNormals;
	GLuint vboTexcoords;
	GLuint ibo;
};

class OpenGLES2Renderer : public Direct3DRMRenderer {
public:
	static Direct3DRMRenderer* Create(DWORD width, DWORD height);
	OpenGLES2Renderer(
		DWORD width,
		DWORD height,
		SDL_GLContext context,
		GLuint fbo,
		GLuint colorTex,
		GLuint vertexBuffer,
		GLuint shaderProgram
	);
	~OpenGLES2Renderer() override;

	void PushLights(const SceneLight* lightsArray, size_t count) override;
	void SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back) override;
	void SetFrustumPlanes(const Plane* frustumPlanes) override;
	Uint32 GetTextureId(IDirect3DRMTexture* texture) override;
	Uint32 GetMeshId(IDirect3DRMMesh* mesh, const MeshGroup* meshGroup) override;
	DWORD GetWidth() override;
	DWORD GetHeight() override;
	void GetDesc(D3DDEVICEDESC* halDesc, D3DDEVICEDESC* helDesc) override;
	const char* GetName() override;
	HRESULT BeginFrame() override;
	void EnableTransparency() override;
	void SubmitDraw(
		DWORD meshId,
		const D3DRMMATRIX4D& modelViewMatrix,
		const Matrix3x3& normalMatrix,
		const Appearance& appearance
	) override;
	HRESULT FinalizeFrame() override;

private:
	void AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture);
	void AddMeshDestroyCallback(Uint32 id, IDirect3DRMMesh* mesh);

	std::vector<GLES2TextureCacheEntry> m_textures;
	std::vector<GLES2MeshCacheEntry> m_meshs;
	D3DRMMATRIX4D m_projection;
	SDL_Surface* m_renderedImage;
	DWORD m_width, m_height;
	std::vector<SceneLight> m_lights;
	SDL_GLContext m_context;
	GLuint m_fbo;
	GLuint m_colorTex;
	GLuint m_depthRb;
	GLuint m_shaderProgram;
};

inline static void OpenGLES2Renderer_EnumDevice(LPD3DENUMDEVICESCALLBACK cb, void* ctx)
{
	Direct3DRMRenderer* device = OpenGLES2Renderer::Create(640, 480);
	if (device) {
		EnumDevice(cb, ctx, device, OpenGLES2_GUID);
		delete device;
	}
}
