#include "d3drmrenderer_opengl1.h"
#include "ddraw_impl.h"
#include "ddsurface_impl.h"
#include "mathutils.h"
#include "meshutils.h"

#include <GL/glew.h>
#include <algorithm>
#include <cstring>
#include <vector>

Direct3DRMRenderer* OpenGL1Renderer::Create(DWORD width, DWORD height)
{
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

	SDL_Window* window = DDWindow;
	bool testWindow = false;
	if (!window) {
		window = SDL_CreateWindow("OpenGL 1.2 test", width, height, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
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

	if (!GLEW_EXT_framebuffer_object) {
		if (testWindow) {
			SDL_DestroyWindow(window);
		}
		return nullptr;
	}

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);

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

	return new OpenGL1Renderer(width, height, context, fbo, colorTex, depthRb);
}

OpenGL1Renderer::OpenGL1Renderer(
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
	m_useVBOs = GLEW_ARB_vertex_buffer_object;
}

OpenGL1Renderer::~OpenGL1Renderer()
{
	if (m_renderedImage) {
		SDL_DestroySurface(m_renderedImage);
	}
}

void OpenGL1Renderer::PushLights(const SceneLight* lightsArray, size_t count)
{
	m_lights.assign(lightsArray, lightsArray + count);
}

void OpenGL1Renderer::SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back)
{
	memcpy(&m_projection, projection, sizeof(D3DRMMATRIX4D));
	m_projection[1][1] *= -1.0f; // OpenGL is upside down
}

struct TextureDestroyContextGL {
	OpenGL1Renderer* renderer;
	Uint32 textureId;
};

void OpenGL1Renderer::AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture)
{
	auto* ctx = new TextureDestroyContextGL{this, id};
	texture->AddDestroyCallback(
		[](IDirect3DRMObject* obj, void* arg) {
			auto* ctx = static_cast<TextureDestroyContextGL*>(arg);
			auto& cache = ctx->renderer->m_textures[ctx->textureId];
			if (cache.glTextureId != 0) {
				glDeleteTextures(1, &cache.glTextureId);
				cache.glTextureId = 0;
				cache.texture = nullptr;
			}
			delete ctx;
		},
		ctx
	);
}

Uint32 OpenGL1Renderer::GetTextureId(IDirect3DRMTexture* iTexture)
{
	auto texture = static_cast<Direct3DRMTextureImpl*>(iTexture);
	auto surface = static_cast<DirectDrawSurfaceImpl*>(texture->m_surface);

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (tex.texture == texture) {
			if (tex.version != texture->m_version) {
				glDeleteTextures(1, &tex.glTextureId);
				glGenTextures(1, &tex.glTextureId);
				glBindTexture(GL_TEXTURE_2D, tex.glTextureId);

				SDL_Surface* surf =
					SDL_ConvertSurface(surface->m_surface, SDL_PIXELFORMAT_ABGR8888); // Why are the colors backwarsd?
				if (!surf) {
					return NO_TEXTURE_ID;
				}
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, surf->w, surf->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surf->pixels);
				SDL_DestroySurface(surf);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

				tex.version = texture->m_version;
			}
			return i;
		}
	}

	GLuint texId;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);

	SDL_Surface* surf =
		SDL_ConvertSurface(surface->m_surface, SDL_PIXELFORMAT_ABGR8888); // Why are the colors backwarsd?
	if (!surf) {
		return NO_TEXTURE_ID;
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, surf->w, surf->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surf->pixels);
	SDL_DestroySurface(surf);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (!tex.texture) {
			tex.texture = texture;
			tex.version = texture->m_version;
			tex.glTextureId = texId;
			AddTextureDestroyCallback(i, texture);
			return i;
		}
	}

	m_textures.push_back({texture, texture->m_version, texId});
	AddTextureDestroyCallback((Uint32) (m_textures.size() - 1), texture);
	return (Uint32) (m_textures.size() - 1);
}

GLMeshCacheEntry GLUploadMesh(const MeshGroup& meshGroup, bool useVBOs)
{
	GLMeshCacheEntry cache{&meshGroup, meshGroup.version};

	cache.flat = meshGroup.quality == D3DRMRENDER_FLAT || meshGroup.quality == D3DRMRENDER_UNLITFLAT;

	std::vector<D3DRMVERTEX> vertices;
	if (cache.flat) {
		FlattenSurfaces(
			meshGroup.vertices.data(),
			meshGroup.vertices.size(),
			meshGroup.indices.data(),
			meshGroup.indices.size(),
			meshGroup.texture != nullptr,
			vertices,
			cache.indices
		);
	}
	else {
		vertices = meshGroup.vertices;
		cache.indices = meshGroup.indices;
	}

	if (meshGroup.texture != nullptr) {
		cache.texcoords.resize(vertices.size());
		std::transform(vertices.begin(), vertices.end(), cache.texcoords.begin(), [](const D3DRMVERTEX& v) {
			return v.texCoord;
		});
	}
	cache.positions.resize(vertices.size());
	std::transform(vertices.begin(), vertices.end(), cache.positions.begin(), [](const D3DRMVERTEX& v) {
		return v.position;
	});
	cache.normals.resize(vertices.size());
	std::transform(vertices.begin(), vertices.end(), cache.normals.begin(), [](const D3DRMVERTEX& v) {
		return v.normal;
	});

	if (useVBOs) {
		glGenBuffers(1, &cache.vboPositions);
		glBindBuffer(GL_ARRAY_BUFFER, cache.vboPositions);
		glBufferData(
			GL_ARRAY_BUFFER,
			cache.positions.size() * sizeof(D3DVECTOR),
			cache.positions.data(),
			GL_STATIC_DRAW
		);

		glGenBuffers(1, &cache.vboNormals);
		glBindBuffer(GL_ARRAY_BUFFER, cache.vboNormals);
		glBufferData(GL_ARRAY_BUFFER, cache.normals.size() * sizeof(D3DVECTOR), cache.normals.data(), GL_STATIC_DRAW);

		if (meshGroup.texture != nullptr) {
			glGenBuffers(1, &cache.vboTexcoords);
			glBindBuffer(GL_ARRAY_BUFFER, cache.vboTexcoords);
			glBufferData(
				GL_ARRAY_BUFFER,
				cache.texcoords.size() * sizeof(TexCoord),
				cache.texcoords.data(),
				GL_STATIC_DRAW
			);
		}

		glGenBuffers(1, &cache.ibo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cache.ibo);
		glBufferData(
			GL_ELEMENT_ARRAY_BUFFER,
			cache.indices.size() * sizeof(Uint32),
			cache.indices.data(),
			GL_STATIC_DRAW
		);
	}

	return cache;
}

struct GLMeshDestroyContext {
	OpenGL1Renderer* renderer;
	Uint32 id;
};

void OpenGL1Renderer::AddMeshDestroyCallback(Uint32 id, IDirect3DRMMesh* mesh)
{
	auto* ctx = new GLMeshDestroyContext{this, id};
	mesh->AddDestroyCallback(
		[](IDirect3DRMObject*, void* arg) {
			auto* ctx = static_cast<GLMeshDestroyContext*>(arg);
			auto& cache = ctx->renderer->m_meshs[ctx->id];
			cache.meshGroup = nullptr;
			if (ctx->renderer->m_useVBOs) {
				glDeleteBuffers(1, &cache.vboPositions);
				glDeleteBuffers(1, &cache.vboNormals);
				glDeleteBuffers(1, &cache.vboTexcoords);
				glDeleteBuffers(1, &cache.ibo);
			}
			delete ctx;
		},
		ctx
	);
}

Uint32 OpenGL1Renderer::GetMeshId(IDirect3DRMMesh* mesh, const MeshGroup* meshGroup)
{
	for (Uint32 i = 0; i < m_meshs.size(); ++i) {
		auto& cache = m_meshs[i];
		if (cache.meshGroup == meshGroup) {
			if (cache.version != meshGroup->version) {
				cache = std::move(GLUploadMesh(*meshGroup, m_useVBOs));
			}
			return i;
		}
	}

	auto newCache = GLUploadMesh(*meshGroup, m_useVBOs);

	for (Uint32 i = 0; i < m_meshs.size(); ++i) {
		auto& cache = m_meshs[i];
		if (!cache.meshGroup) {
			cache = std::move(newCache);
			AddMeshDestroyCallback(i, mesh);
			return i;
		}
	}

	m_meshs.push_back(std::move(newCache));
	AddMeshDestroyCallback((Uint32) (m_meshs.size() - 1), mesh);
	return (Uint32) (m_meshs.size() - 1);
}

DWORD OpenGL1Renderer::GetWidth()
{
	return m_width;
}

DWORD OpenGL1Renderer::GetHeight()
{
	return m_height;
}

void OpenGL1Renderer::GetDesc(D3DDEVICEDESC* halDesc, D3DDEVICEDESC* helDesc)
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

const char* OpenGL1Renderer::GetName()
{
	return "OpenGL 1.2 HAL";
}

HRESULT OpenGL1Renderer::BeginFrame(const D3DRMMATRIX4D& viewMatrix)
{
	if (!DDBackBuffer) {
		return DDERR_GENERIC;
	}

	memcpy(m_viewMatrix, viewMatrix, sizeof(D3DRMMATRIX4D));

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

	return DD_OK;
}

void OpenGL1Renderer::SubmitDraw(
	DWORD meshId,
	const D3DRMMATRIX4D& worldMatrix,
	const Matrix3x3& normalMatrix,
	const Appearance& appearance
)
{
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	D3DRMMATRIX4D mvMatrix;
	MultiplyMatrix(mvMatrix, worldMatrix, m_viewMatrix);
	glLoadMatrixf(&mvMatrix[0][0]);
	glEnable(GL_NORMALIZE);

	glColor4ub(appearance.color.r, appearance.color.g, appearance.color.b, appearance.color.a);

	float shininess = appearance.shininess * m_shininessFactor;
	glMaterialf(GL_FRONT, GL_SHININESS, shininess);
	if (shininess != 0.0f) {
		GLfloat whiteSpec[] = {shininess, shininess, shininess, shininess};
		glMaterialfv(GL_FRONT, GL_SPECULAR, whiteSpec);
	}
	else {
		GLfloat noSpec[] = {0.0f, 0.0f, 0.0f, 0.0f};
		glMaterialfv(GL_FRONT, GL_SPECULAR, noSpec);
	}

	auto& mesh = m_meshs[meshId];

	if (mesh.flat) {
		glShadeModel(GL_FLAT);
	}
	else {
		glShadeModel(GL_SMOOTH);
	}

	// Bind texture if present
	if (appearance.textureId != NO_TEXTURE_ID) {
		auto& tex = m_textures[appearance.textureId];
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, tex.glTextureId);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	else {
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);

	if (m_useVBOs) {
		glBindBuffer(GL_ARRAY_BUFFER, mesh.vboPositions);
		glVertexPointer(3, GL_FLOAT, 0, nullptr);

		glBindBuffer(GL_ARRAY_BUFFER, mesh.vboNormals);
		glNormalPointer(GL_FLOAT, 0, nullptr);

		if (appearance.textureId != NO_TEXTURE_ID) {
			glBindBuffer(GL_ARRAY_BUFFER, mesh.vboTexcoords);
			glTexCoordPointer(2, GL_FLOAT, 0, nullptr);
		}

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ibo);
		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_INT, nullptr);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
	else {
		glVertexPointer(3, GL_FLOAT, 0, mesh.positions.data());
		glNormalPointer(GL_FLOAT, 0, mesh.normals.data());
		if (appearance.textureId != NO_TEXTURE_ID) {
			glTexCoordPointer(2, GL_FLOAT, 0, mesh.texcoords.data());
		}

		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_INT, mesh.indices.data());
	}

	glPopMatrix();
}

HRESULT OpenGL1Renderer::FinalizeFrame()
{
	glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, m_renderedImage->pixels);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Composite onto SDL backbuffer
	SDL_BlitSurface(m_renderedImage, nullptr, DDBackBuffer, nullptr);

	return DD_OK;
}
