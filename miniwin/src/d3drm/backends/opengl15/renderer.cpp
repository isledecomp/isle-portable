#include "d3drmrenderer_opengl15.h"
#include "ddraw_impl.h"

#include <GL/glew.h>
#include <cstring>
#include <vector>

Direct3DRMRenderer* OpenGL15Renderer::Create(DWORD width, DWORD height)
{
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

	SDL_Window* window = DDWindow;
	bool testWindow = false;
	if (!window) {
		window = SDL_CreateWindow("OpenGL 1.5 test", width, height, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
		testWindow = true;
	}

	SDL_GLContext context = SDL_GL_CreateContext(window);
	if (!context) {
		if (testWindow) {
			SDL_DestroyWindow(window);
		}
		return nullptr;
	}

	SDL_GL_MakeCurrent(window, context);
	GLenum err = glewInit();
	if (err != GLEW_OK) {
		if (testWindow) {
			SDL_DestroyWindow(window);
		}
		return nullptr;
	}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_COLOR_MATERIAL);

	// Setup FBO
	GLuint fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	// Create color texture
	GLuint colorTex;
	glGenTextures(1, &colorTex);
	glBindTexture(GL_TEXTURE_2D, colorTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

	// Create depth renderbuffer
	GLuint depthRb;
	glGenRenderbuffers(1, &depthRb);
	glBindRenderbuffer(GL_RENDERBUFFER, depthRb);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRb);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		return nullptr;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (testWindow) {
		SDL_DestroyWindow(window);
	}

	return new OpenGL15Renderer(width, height, context, fbo, colorTex, depthRb);
}

OpenGL15Renderer::OpenGL15Renderer(
	int width,
	int height,
	SDL_GLContext context,
	GLuint fbo,
	GLuint colorTex,
	GLuint depthRb
)
	: m_width(width), m_height(height), m_context(context), m_fbo(fbo), m_colorTex(colorTex), m_depthRb(depthRb)
{
	m_renderedImage = SDL_CreateSurface(m_width, m_height, SDL_PIXELFORMAT_ABGR8888);
}

OpenGL15Renderer::~OpenGL15Renderer()
{
	if (m_renderedImage) {
		SDL_DestroySurface(m_renderedImage);
	}
}

void OpenGL15Renderer::SetBackbuffer(SDL_Surface* surface)
{
	m_backbuffer = surface;
}

void OpenGL15Renderer::PushVertices(const PositionColorVertex* verts, size_t count)
{
	m_vertices.assign(verts, verts + count);
}

void OpenGL15Renderer::PushLights(const SceneLight* lightsArray, size_t count)
{
	m_lights.assign(lightsArray, lightsArray + count);
}

void OpenGL15Renderer::SetProjection(D3DRMMATRIX4D perspective, D3DVALUE front, D3DVALUE back)
{
	memcpy(&m_projection, perspective, sizeof(D3DRMMATRIX4D));
	m_projection[1][1] *= -1.0f; // OpenGL is upside down
}

Uint32 OpenGL15Renderer::GetTextureId(IDirect3DRMTexture* texture)
{
	return NO_TEXTURE_ID; // Stub
}

DWORD OpenGL15Renderer::GetWidth()
{
	return m_width;
}

DWORD OpenGL15Renderer::GetHeight()
{
	return m_height;
}

void OpenGL15Renderer::GetDesc(D3DDEVICEDESC* halDesc, D3DDEVICEDESC* helDesc)
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

const char* OpenGL15Renderer::GetName()
{
	return "OpenGL 1.5 Renderer";
}

HRESULT OpenGL15Renderer::Render()
{
	if (!m_backbuffer) {
		return DDERR_GENERIC;
	}
	SDL_GL_MakeCurrent(DDWindow, m_context);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glViewport(0, 0, m_width, m_height);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Disable all lights and reset global ambient
	for (int i = 0; i < 8; ++i) {
		glDisable(GL_LIGHT0 + i);
	}
	const GLfloat zeroAmbient[4] = {0.f, 0.f, 0.f, 1.f};
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, zeroAmbient);

	// Setup lights
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	int lightIdx = 0;
	for (const auto& l : m_lights) {
		if (lightIdx > 7) {
			break;
		}
		GLenum lightId = GL_LIGHT0 + lightIdx++;
		const FColor& c = l.color;
		GLfloat col[4] = {c.r, c.g, c.b, c.a};
		GLfloat pos[4];

		if (l.positional == 0.f && l.directional == 0.f) {
			// Ambient light only
			glLightfv(lightId, GL_AMBIENT, col);
			const GLfloat black[4] = {0.f, 0.f, 0.f, 1.f};
			glLightfv(lightId, GL_DIFFUSE, black);
			glLightfv(lightId, GL_SPECULAR, black);
			const GLfloat dummyPos[4] = {0.f, 0.f, 1.f, 0.f};
			glLightfv(lightId, GL_POSITION, dummyPos);
		}
		else {
			glLightfv(lightId, GL_AMBIENT, zeroAmbient);
			glLightfv(lightId, GL_DIFFUSE, col);
			glLightfv(lightId, GL_SPECULAR, col);

			if (l.directional == 1.f) {
				pos[0] = -l.direction.x;
				pos[1] = -l.direction.y;
				pos[2] = -l.direction.z;
				pos[3] = 0.f;
			}
			else {
				pos[0] = l.position.x;
				pos[1] = l.position.y;
				pos[2] = l.position.z;
				pos[3] = 1.f;
			}
			glLightfv(lightId, GL_POSITION, pos);
		}
		glEnable(lightId);
	}
	glPopMatrix();

	// Projection and view
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(&m_projection[0][0]);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// Render geometry
	glBegin(GL_TRIANGLES);
	for (const auto& v : m_vertices) {
		glColor4ub(v.colors.r, v.colors.g, v.colors.b, v.colors.a);
		glNormal3f(v.normals.x, v.normals.y, v.normals.z);
		glTexCoord2f(v.texCoord.u, v.texCoord.v);

		// Set per-vertex specular material
		glMaterialf(GL_FRONT, GL_SHININESS, v.shininess);
		if (v.shininess != 0.0f) {
			GLfloat whiteSpec[] = {v.shininess, v.shininess, v.shininess, v.shininess};
			glMaterialfv(GL_FRONT, GL_SPECULAR, whiteSpec);
		}
		else {
			GLfloat noSpec[] = {0.f, 0.f, 0.f, 1.f};
			glMaterialfv(GL_FRONT, GL_SPECULAR, noSpec);
		}

		glVertex3f(v.position.x, v.position.y, v.position.z);
	}
	glEnd();

	glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, m_renderedImage->pixels);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Composite onto SDL backbuffer
	SDL_BlitSurface(m_renderedImage, nullptr, m_backbuffer, nullptr);

	return DD_OK;
}
