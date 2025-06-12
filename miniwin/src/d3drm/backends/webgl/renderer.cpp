#include "d3drmrenderer_webgl.h"
#include "meshutils.h"

#include <algorithm>

static GLuint CompileShader(GLenum type, const char* source)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);
	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glDeleteShader(shader);
		SDL_Log("CompileShader (%s)", SDL_GetError());
		return 0;
	}
	return shader;
}

Direct3DRMRenderer* WebGLRenderer::Create(DWORD width, DWORD height)
{
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

	SDL_Window* window = DDWindow;
	if (!window) {
		SDL_Log("Assuming WebGL works");
		return new WebGLRenderer(width, height, {}, 0, 0, 0, 0);
	}

	SDL_GLContext context = SDL_GL_CreateContext(DDWindow);
	if (!context) {
		SDL_Log("SDL_GL_CreateContext failed: %s", SDL_GetError());
		return nullptr;
	}
	SDL_Log("GL_VERSION: %s", glGetString(GL_VERSION));
	SDL_Log("GL_VENDOR: %s", glGetString(GL_VENDOR));

	SDL_GL_MakeCurrent(DDWindow, context);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glClearColor(0.1f, 0.1f, 0.2f, 1.0f);

	const char* vertexShaderSource = R"(
		attribute vec3 position;
		void main() {
			gl_Position = vec4(position, 1.0);
		}
	)";

	const char* fragmentShaderSource = R"(
		precision mediump float;
		void main() {
			gl_FragColor = vec4(1.0, 0.5, 0.2, 1.0);
		}
	)";

	GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexShaderSource);
	GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vs);
	glAttachShader(shaderProgram, fs);
	glBindAttribLocation(shaderProgram, 0, "position");
	glLinkProgram(shaderProgram);
	glDeleteShader(vs);
	glDeleteShader(fs);

	static const GLfloat triangleVertices[] = {0.0f, 0.5f, 0.0f, -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f};
	GLuint vertexBuffer;
	glGenBuffers(1, &vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(triangleVertices), triangleVertices, GL_STATIC_DRAW);

	return new WebGLRenderer(width, height, context, shaderProgram, 0, 0, vertexBuffer);
}

WebGLRenderer::WebGLRenderer(
	DWORD width,
	DWORD height,
	SDL_GLContext glContext,
	GLuint shaderProgram,
	GLuint fbo,
	GLuint colorTex,
	GLuint vertexBuffer
)
	: m_width(width), m_height(height), m_glContext(glContext), m_shaderProgram(shaderProgram), m_fbo(fbo),
	  m_colorTex(colorTex), m_vertexBuffer(vertexBuffer)
{
}

WebGLRenderer::~WebGLRenderer()
{
	if (m_colorTex) {
		glDeleteTextures(1, &m_colorTex);
	}
	if (m_fbo) {
		glDeleteFramebuffers(1, &m_fbo);
	}

	if (m_vertexBuffer) {
		glDeleteBuffers(1, &m_vertexBuffer);
	}
	if (m_shaderProgram) {
		glDeleteProgram(m_shaderProgram);
	}
}

void WebGLRenderer::PushLights(const SceneLight* lightsArray, size_t count)
{
	m_lights.assign(lightsArray, lightsArray + count);
}

void WebGLRenderer::SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back)
{
	memcpy(&m_projection, projection, sizeof(D3DRMMATRIX4D));
	m_projection[1][1] *= -1.0f; // WebGL is upside down
}

Uint32 WebGLRenderer::GetTextureId(IDirect3DRMTexture* texture)
{
	return 0;
}

Uint32 WebGLRenderer::GetMeshId(IDirect3DRMMesh* mesh, const MeshGroup* meshGroup)
{
	return 0;
}

DWORD WebGLRenderer::GetWidth()
{
	return m_width;
}
DWORD WebGLRenderer::GetHeight()
{
	return m_height;
}

void WebGLRenderer::GetDesc(D3DDEVICEDESC* halDesc, D3DDEVICEDESC* helDesc)
{
	halDesc->dcmColorModel = D3DCOLORMODEL::RGB;
	halDesc->dwFlags = D3DDD_DEVICEZBUFFERBITDEPTH;
	halDesc->dwDeviceZBufferBitDepth = DDBD_24; // Todo add support for other depths
	helDesc->dwDeviceRenderBitDepth = DDBD_8 | DDBD_16 | DDBD_24 | DDBD_32;
	halDesc->dpcTriCaps.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE;
	halDesc->dpcTriCaps.dwShadeCaps = D3DPSHADECAPS_ALPHAFLATBLEND;
	halDesc->dpcTriCaps.dwTextureFilterCaps = D3DPTFILTERCAPS_LINEAR;

	memset(helDesc, 0, sizeof(D3DDEVICEDESC));
}
const char* WebGLRenderer::GetName()
{
	return "WebGL HAL";
}

HRESULT WebGLRenderer::BeginFrame(const D3DRMMATRIX4D&)
{
	glViewport(0, 0, m_width, m_height);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	return DD_OK;
}

void WebGLRenderer::EnableTransparency()
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void WebGLRenderer::SubmitDraw(
	DWORD meshId,
	const D3DRMMATRIX4D& worldMatrix,
	const Matrix3x3& normalMatrix,
	const Appearance& appearance
)
{
	if (!m_shaderProgram || !m_vertexBuffer) {
		return;
	}

	glUseProgram(m_shaderProgram);

	glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
	glEnableVertexAttribArray(0); // position attribute index 0
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	glDisableVertexAttribArray(0);
	glUseProgram(0);
}

HRESULT WebGLRenderer::FinalizeFrame()
{
	SDL_GL_SwapWindow(DDWindow);
	return DD_OK;
}
