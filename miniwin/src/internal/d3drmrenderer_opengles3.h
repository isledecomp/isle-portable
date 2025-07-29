#pragma once

#include "d3drmrenderer.h"
#include "d3drmtexture_impl.h"
#include "ddraw_impl.h"

#include <GLES3/gl3.h>
#include <SDL3/SDL.h>
#include <vector>

DEFINE_GUID(OpenGLES3_GUID, 0x682656F3, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04);

struct GLES3TextureCacheEntry {
	IDirect3DRMTexture* texture;
	Uint32 version;
	GLuint glTextureId;
	uint16_t width;
	uint16_t height;
};

struct GLES3MeshCacheEntry {
	const MeshGroup* meshGroup;
	int version;
	bool flat;

	std::vector<uint16_t> indices;
	GLuint vboPositions;
	GLuint vboNormals;
	GLuint vboTexcoords;
	GLuint ibo;
	GLuint vao;
};

class OpenGLES3Renderer : public Direct3DRMRenderer {
public:
	static Direct3DRMRenderer* Create(DWORD width, DWORD height, DWORD msaaSamples, float anisotropic);
	OpenGLES3Renderer(
		DWORD width,
		DWORD height,
		DWORD msaaSamples,
		float anisotropic,
		SDL_GLContext context,
		GLuint shaderProgram
	);
	~OpenGLES3Renderer() override;

	void PushLights(const SceneLight* lightsArray, size_t count) override;
	void SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back) override;
	void SetFrustumPlanes(const Plane* frustumPlanes) override;
	Uint32 GetTextureId(IDirect3DRMTexture* texture, bool isUI, float scaleX, float scaleY) override;
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

private:
	void AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture);
	void AddMeshDestroyCallback(Uint32 id, IDirect3DRMMesh* mesh);
	GLES3MeshCacheEntry GLES3UploadMesh(const MeshGroup& meshGroup, bool forceUV = false);
	bool UploadTexture(SDL_Surface* source, GLuint& outTexId, bool isUI);

	MeshGroup m_uiMesh;
	GLES3MeshCacheEntry m_uiMeshCache;
	std::vector<GLES3TextureCacheEntry> m_textures;
	std::vector<GLES3MeshCacheEntry> m_meshs;
	D3DRMMATRIX4D m_projection;
	SDL_Surface* m_renderedImage = nullptr;
	bool m_dirty = false;
	std::vector<SceneLight> m_lights;
	SDL_GLContext m_context;
	uint32_t m_msaa;
	float m_anisotropic;
	GLuint m_fbo;
	GLuint m_resolveFBO;
	GLuint m_colorTarget;
	GLuint m_resolveColor = 0;
	GLuint m_depthTarget;
	GLuint m_shaderProgram;
	GLuint m_dummyTexture;
	GLint m_posLoc;
	GLint m_normLoc;
	GLint m_texLoc;
	GLint m_colorLoc;
	GLint m_shinLoc;
	GLint m_lightCountLoc;
	GLint m_useTextureLoc;
	GLint m_textureLoc;
	GLint u_lightLocs[3][3];
	GLint m_modelViewMatrixLoc;
	GLint m_normalMatrixLoc;
	GLint m_projectionMatrixLoc;
	ViewportTransform m_viewportTransform;
};

inline static void OpenGLES3Renderer_EnumDevice(const IDirect3DMiniwin* d3d, LPD3DENUMDEVICESCALLBACK cb, void* ctx)
{
	Direct3DRMRenderer* device = OpenGLES3Renderer::Create(640, 480, d3d->GetMSAASamples(), d3d->GetAnisotropic());
	if (!device) {
		return;
	}

	delete device;

	D3DDEVICEDESC halDesc = {};
	halDesc.dcmColorModel = D3DCOLOR_RGB;
	halDesc.dwFlags = D3DDD_DEVICEZBUFFERBITDEPTH;
	halDesc.dwDeviceZBufferBitDepth = DDBD_24;
	halDesc.dwDeviceRenderBitDepth = DDBD_32;
	halDesc.dpcTriCaps.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE;
	halDesc.dpcTriCaps.dwShadeCaps = D3DPSHADECAPS_ALPHAFLATBLEND;
	halDesc.dpcTriCaps.dwTextureFilterCaps = D3DPTFILTERCAPS_LINEAR;

	D3DDEVICEDESC helDesc = {};

	EnumDevice(cb, ctx, "OpenGL ES 3.0 HAL", &halDesc, &helDesc, OpenGLES3_GUID);
}
