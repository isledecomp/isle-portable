#pragma once

#include "d3drmrenderer.h"
#include "d3drmtexture_impl.h"
#include "ddraw_impl.h"

#include <GL/glew.h>
#include <SDL3/SDL.h>
#include <vector>

DEFINE_GUID(OPENGL15_GUID, 0x682656F3, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03);

class OpenGL15Renderer : public Direct3DRMRenderer {
public:
	static Direct3DRMRenderer* Create(DWORD width, DWORD height);
	OpenGL15Renderer(int width, int height, SDL_GLContext context, GLuint fbo, GLuint colorTex, GLuint depthRb);
	~OpenGL15Renderer() override;
	void PushVertices(const PositionColorVertex* verts, size_t count) override;
	void PushLights(const SceneLight* lightsArray, size_t count) override;
	void SetProjection(D3DRMMATRIX4D perspective, D3DVALUE front, D3DVALUE back) override;
	Uint32 GetTextureId(IDirect3DRMTexture* texture) override;
	DWORD GetWidth() override;
	DWORD GetHeight() override;
	void GetDesc(D3DDEVICEDESC* halDesc, D3DDEVICEDESC* helDesc) override;
	const char* GetName() override;
	HRESULT Render() override;

private:
	SDL_GLContext m_context;
	D3DRMMATRIX4D m_projection;
	SDL_Surface* m_renderedImage;
	int m_width, m_height;
	std::vector<PositionColorVertex> m_vertices;
	std::vector<SceneLight> m_lights;
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
