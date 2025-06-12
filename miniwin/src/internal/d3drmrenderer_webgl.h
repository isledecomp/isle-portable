#pragma once

#include "d3drmrenderer.h"
#include "d3drmtexture_impl.h"
#include "ddraw_impl.h"

#include <SDL3/SDL.h>
#ifdef __EMSCRIPTEN__
#include <GLES2/gl2.h>
#include <emscripten/html5.h>
#else
#include <GL/glew.h>
#endif
#include <unordered_map>
#include <vector>

DEFINE_GUID(WebGL_GUID, 0x682656F3, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04);

class WebGLRenderer : public Direct3DRMRenderer {
public:
	static Direct3DRMRenderer* Create(DWORD width, DWORD height);
	WebGLRenderer(
		DWORD width,
		DWORD height,
		SDL_GLContext glContext,
		GLuint shaderProgram,
		GLuint fbo,
		GLuint colorTex,
		GLuint vertexBuffer
	);
	~WebGLRenderer() override;

	void PushLights(const SceneLight* lightsArray, size_t count) override;
	void SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back) override;
	Uint32 GetTextureId(IDirect3DRMTexture* texture) override;
	Uint32 GetMeshId(IDirect3DRMMesh* mesh, const MeshGroup* meshGroup) override;
	DWORD GetWidth() override;
	DWORD GetHeight() override;
	void GetDesc(D3DDEVICEDESC* halDesc, D3DDEVICEDESC* helDesc) override;
	const char* GetName() override;

	HRESULT BeginFrame(const D3DRMMATRIX4D& viewMatrix) override;
	void EnableTransparency() override;
	void SubmitDraw(
		DWORD meshId,
		const D3DRMMATRIX4D& worldMatrix,
		const Matrix3x3& normalMatrix,
		const Appearance& appearance
	) override;
	HRESULT FinalizeFrame() override;

private:
	D3DRMMATRIX4D m_viewMatrix;
	D3DRMMATRIX4D m_projection;
	std::vector<SceneLight> m_lights;

	DWORD m_width;
	DWORD m_height;
	SDL_Surface* m_renderedImage;
	GLuint m_shaderProgram = 0;
	GLuint m_vertexBuffer = 0;
	GLuint m_fbo = 0;
	GLuint m_colorTex = 0;

	SDL_GLContext m_glContext;
};

inline static void WebGLRenderer_EnumDevice(LPD3DENUMDEVICESCALLBACK cb, void* ctx)
{
	Direct3DRMRenderer* device = WebGLRenderer::Create(640, 480);
	if (device) {
		EnumDevice(cb, ctx, device, WebGL_GUID);
		delete device;
	}
}
