#pragma once

#include "d3drmrenderer.h"
#include "d3drmtexture_impl.h"
#include "ddraw_impl.h"

#include <GL/glew.h>
#include <SDL3/SDL.h>
#include <vector>

DEFINE_GUID(OPENGL15_GUID, 0x682656F3, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03);

struct GLTextureCacheEntry {
	IDirect3DRMTexture* texture;
	Uint32 version;
	GLuint glTextureId;
};

class OpenGL15Renderer : public Direct3DRMRenderer {
public:
	static Direct3DRMRenderer* Create(DWORD width, DWORD height);
	OpenGL15Renderer(int width, int height, SDL_GLContext context, GLuint fbo, GLuint colorTex, GLuint depthRb);
	~OpenGL15Renderer() override;
	void PushLights(const SceneLight* lightsArray, size_t count) override;
	void SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back) override;
	Uint32 GetTextureId(IDirect3DRMTexture* texture) override;
	DWORD GetWidth() override;
	DWORD GetHeight() override;
	void GetDesc(D3DDEVICEDESC* halDesc, D3DDEVICEDESC* helDesc) override;
	const char* GetName() override;
	HRESULT BeginFrame(const D3DRMMATRIX4D& viewMatrix) override;
	void SubmitDraw(
		const D3DRMVERTEX* vertices,
		const size_t count,
		const D3DRMMATRIX4D& worldMatrix,
		const Matrix3x3& normalMatrix,
		const Appearance& appearance
	) override;
	HRESULT FinalizeFrame() override;
	HRESULT GetDescription(char* buffer, int bufferSize) override;

private:
	void AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture);
	std::vector<GLTextureCacheEntry> m_textures;
	D3DRMMATRIX4D m_viewMatrix;
	D3DRMMATRIX4D m_projection;
	SDL_Surface* m_renderedImage;
	int m_width, m_height;
	std::vector<SceneLight> m_lights;
	SDL_GLContext m_context;
	GLuint m_fbo = 0;
	GLuint m_colorTex = 0;
	GLuint m_depthRb = 0;
};

inline static void OpenGL15Renderer_EnumDevice(LPD3DENUMDEVICESCALLBACK cb, void* ctx)
{
	Direct3DRMRenderer* device = OpenGL15Renderer::Create(640, 480);
	if (device) {
		EnumDevice(cb, ctx, device, OPENGL15_GUID);
		delete device;
	}
}
