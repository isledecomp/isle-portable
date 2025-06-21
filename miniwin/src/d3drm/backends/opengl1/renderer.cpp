#include <GL/glew.h>
// must come after GLEW
#include "d3drmrenderer_opengl1.h"
#include "ddraw_impl.h"
#include "ddsurface_impl.h"
#include "mathutils.h"
#include "meshutils.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cstring>
#include <vector>

Direct3DRMRenderer* OpenGL1Renderer::Create(DWORD width, DWORD height)
{
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

	SDL_Window* window = DDWindow;
	bool testWindow = false;
	if (!window) {
		window = SDL_CreateWindow("OpenGL 1.1 test", width, height, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
		testWindow = true;
	}

	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	SDL_GLContext context = SDL_GL_CreateContext(window);
	if (!context) {
		if (testWindow) {
			SDL_DestroyWindow(window);
		}
		return nullptr;
	}

	if (!SDL_GL_MakeCurrent(window, context)) {
		if (testWindow) {
			SDL_DestroyWindow(window);
		}
		return nullptr;
	}

	GLenum err = glewInit();
	if (err != GLEW_OK) {
		if (testWindow) {
			SDL_DestroyWindow(window);
		}
		return nullptr;
	}

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CW);

	if (testWindow) {
		SDL_DestroyWindow(window);
	}

	return new OpenGL1Renderer(width, height, context);
}

OpenGL1Renderer::OpenGL1Renderer(DWORD width, DWORD height, SDL_GLContext context) : m_context(context)
{
	m_width = width;
	m_height = height;
	m_renderedImage = SDL_CreateSurface(m_width, m_height, SDL_PIXELFORMAT_RGBA32);
	m_useVBOs = GLEW_ARB_vertex_buffer_object;
	SDL_GL_MakeCurrent(DDWindow, m_context);
	glViewport(0, 0, m_width, m_height);
	m_virtualWidth = 640;
	m_virtualHeight = 480;
}

OpenGL1Renderer::~OpenGL1Renderer()
{
	SDL_DestroySurface(m_renderedImage);
}

void OpenGL1Renderer::PushLights(const SceneLight* lightsArray, size_t count)
{
	m_lights.assign(lightsArray, lightsArray + count);
}

void OpenGL1Renderer::SetFrustumPlanes(const Plane* frustumPlanes)
{
}

void OpenGL1Renderer::SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back)
{
	memcpy(&m_projection, projection, sizeof(D3DRMMATRIX4D));
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

				SDL_Surface* surf = SDL_ConvertSurface(surface->m_surface, SDL_PIXELFORMAT_RGBA32);
				if (!surf) {
					return NO_TEXTURE_ID;
				}
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surf->w, surf->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surf->pixels);
				SDL_DestroySurface(surf);

				tex.version = texture->m_version;
			}
			return i;
		}
	}

	GLuint texId;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);

	SDL_Surface* surf = SDL_ConvertSurface(surface->m_surface, SDL_PIXELFORMAT_RGBA32);
	if (!surf) {
		return NO_TEXTURE_ID;
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surf->w, surf->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surf->pixels);
	SDL_DestroySurface(surf);

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
		cache.indices.resize(meshGroup.indices.size());
		std::transform(meshGroup.indices.begin(), meshGroup.indices.end(), cache.indices.begin(), [](DWORD index) {
			return static_cast<uint16_t>(index);
		});
	}

	if (meshGroup.texture) {
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
			cache.indices.size() * sizeof(cache.indices[0]),
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

void OpenGL1Renderer::GetDesc(D3DDEVICEDESC* halDesc, D3DDEVICEDESC* helDesc)
{
	halDesc->dcmColorModel = D3DCOLORMODEL::RGB;
	halDesc->dwFlags = D3DDD_DEVICEZBUFFERBITDEPTH;
	halDesc->dwDeviceZBufferBitDepth = DDBD_24;
	helDesc->dwDeviceRenderBitDepth = DDBD_32;
	halDesc->dpcTriCaps.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE;
	halDesc->dpcTriCaps.dwShadeCaps = D3DPSHADECAPS_ALPHAFLATBLEND;
	halDesc->dpcTriCaps.dwTextureFilterCaps = D3DPTFILTERCAPS_LINEAR;

	memset(helDesc, 0, sizeof(D3DDEVICEDESC));
}

const char* OpenGL1Renderer::GetName()
{
	return "OpenGL 1.1 HAL";
}

HRESULT OpenGL1Renderer::BeginFrame()
{
	m_dirty = true;
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glEnable(GL_LIGHTING);
	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

	// Disable all lights and reset global ambient
	for (int i = 0; i < 8; ++i) {
		glDisable(GL_LIGHT0 + i);
	}
	const GLfloat zeroAmbient[4] = {0.f, 0.f, 0.f, 1.f};
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, zeroAmbient);
	glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);

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
			if (l.directional == 1.0f) {
				glLightfv(lightId, GL_SPECULAR, col);
			}
			else {
				const GLfloat black[4] = {0.f, 0.f, 0.f, 1.f};
				glLightfv(lightId, GL_SPECULAR, black);
			}

			GLfloat pos[4];
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

void OpenGL1Renderer::EnableTransparency()
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);
}

void OpenGL1Renderer::SubmitDraw(
	DWORD meshId,
	const D3DRMMATRIX4D& modelViewMatrix,
	const Matrix3x3& normalMatrix,
	const Appearance& appearance
)
{
	auto& mesh = m_meshs[meshId];

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	glLoadMatrixf(&modelViewMatrix[0][0]);
	glEnable(GL_NORMALIZE);

	glColor4ub(appearance.color.r, appearance.color.g, appearance.color.b, appearance.color.a);

	if (appearance.shininess != 0.0f) {
		GLfloat whiteSpec[] = {1.f, 1.f, 1.f, 1.f};
		glMaterialfv(GL_FRONT, GL_SPECULAR, whiteSpec);
		glMaterialf(GL_FRONT, GL_SHININESS, appearance.shininess);
	}
	else {
		GLfloat noSpec[] = {0.0f, 0.0f, 0.0f, 0.0f};
		glMaterialfv(GL_FRONT, GL_SPECULAR, noSpec);
		glMaterialf(GL_FRONT, GL_SHININESS, 0.0f);
	}

	if (mesh.flat) {
		glShadeModel(GL_FLAT);
	}
	else {
		glShadeModel(GL_SMOOTH);
	}

	// Bind texture if present
	if (appearance.textureId != NO_TEXTURE_ID) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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
		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_SHORT, nullptr);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
	else {
		glVertexPointer(3, GL_FLOAT, 0, mesh.positions.data());
		glNormalPointer(GL_FLOAT, 0, mesh.normals.data());
		if (appearance.textureId != NO_TEXTURE_ID) {
			glTexCoordPointer(2, GL_FLOAT, 0, mesh.texcoords.data());
		}

		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_SHORT, mesh.indices.data());
	}

	glPopMatrix();
}

HRESULT OpenGL1Renderer::FinalizeFrame()
{
	return DD_OK;
}

struct ViewportTransform {
	float scale;
	float offsetX;
	float offsetY;
};

ViewportTransform CalculateViewportTransform(int virtualW, int virtualH, int windowW, int windowH)
{
	float scaleX = (float) windowW / virtualW;
	float scaleY = (float) windowH / virtualH;
	float scale = (scaleX < scaleY) ? scaleX : scaleY;

	float viewportW = virtualW * scale;
	float viewportH = virtualH * scale;

	float offsetX = (windowW - viewportW) * 0.5f;
	float offsetY = (windowH - viewportH) * 0.5f;

	return {scale, offsetX, offsetY};
}

bool OpenGL1Renderer::ConvertEventToRenderCoordinates(SDL_Event* event)
{
	switch (event->type) {
	case SDL_EVENT_MOUSE_MOTION:
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP: {
		ViewportTransform vp = CalculateViewportTransform(m_virtualWidth, m_virtualHeight, m_width, m_height);
		int rawX = event->motion.x;
		int rawY = event->motion.y;
		float x = (rawX - vp.offsetX) / vp.scale;
		float y = (rawY - vp.offsetY) / vp.scale;
		event->motion.x = static_cast<Sint32>(x);
		event->motion.y = static_cast<Sint32>(y);
		break;
	} break;
	case SDL_EVENT_WINDOW_RESIZED:
		m_width = event->window.data1;
		m_height = event->window.data2;
		SDL_DestroySurface(m_renderedImage);
		m_renderedImage = SDL_CreateSurface(m_width, m_height, SDL_PIXELFORMAT_RGBA32);
		glViewport(0, 0, m_width, m_height);
		break;
	}

	return true;
}

void OpenGL1Renderer::Clear(float r, float g, float b)
{
	m_dirty = true;
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearColor(r, g, b, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGL1Renderer::Flip()
{
	if (m_dirty) {
		SDL_GL_SwapWindow(DDWindow);
		m_dirty = false;
	}
}

void OpenGL1Renderer::Draw2DImage(Uint32 textureId, const SDL_Rect& srcRect, const SDL_Rect& dstRect)
{
	m_dirty = true;
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	ViewportTransform vp = CalculateViewportTransform(m_virtualWidth, m_virtualHeight, m_width, m_height);
	float left = -vp.offsetX / vp.scale;
	float right = (m_width - vp.offsetX) / vp.scale;
	float top = -vp.offsetY / vp.scale;
	float bottom = (m_height - vp.offsetY) / vp.scale;
	glOrtho(left, right, bottom, top, -1, 1);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glDisable(GL_LIGHTING);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, m_textures[textureId].glTextureId);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Normalize source rect to texture coords

	GLint boundTexture = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);

	GLfloat texW, texH;
	glGetTexLevelParameterfv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texW);
	glGetTexLevelParameterfv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texH);

	float u1 = srcRect.x / texW;
	float v1 = srcRect.y / texH;
	float u2 = (srcRect.x + srcRect.w) / texW;
	float v2 = (srcRect.y + srcRect.h) / texH;

	float x1 = (float) dstRect.x;
	float y1 = (float) dstRect.y;
	float x2 = x1 + dstRect.w;
	float y2 = y1 + dstRect.h;

	glBegin(GL_QUADS);
	glTexCoord2f(u1, v1);
	glVertex2f(x1, y1);
	glTexCoord2f(u2, v1);
	glVertex2f(x2, y1);
	glTexCoord2f(u2, v2);
	glVertex2f(x2, y2);
	glTexCoord2f(u1, v2);
	glVertex2f(x1, y2);
	glEnd();

	// Restore state
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
}
void OpenGL1Renderer::Download(SDL_Surface* target)
{
	glFinish();
	glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, m_renderedImage->pixels);

	ViewportTransform vp = CalculateViewportTransform(m_virtualWidth, m_virtualHeight, m_width, m_height);

	SDL_Rect srcRect = {
		static_cast<int>(vp.offsetX),
		static_cast<int>(vp.offsetY),
		static_cast<int>(m_virtualWidth * vp.scale),
		static_cast<int>(m_virtualHeight * vp.scale),
	};

	SDL_Surface* bufferClone = SDL_CreateSurface(m_virtualWidth, m_virtualHeight, SDL_PIXELFORMAT_RGBA32);
	if (!bufferClone) {
		SDL_Log("SDL_CreateSurface: %s", SDL_GetError());
		return;
	}

	SDL_BlitSurfaceScaled(m_renderedImage, &srcRect, bufferClone, nullptr, SDL_SCALEMODE_NEAREST);

	// Flip image vertically into target
	SDL_Rect rowSrc = {0, 0, bufferClone->w, 1};
	SDL_Rect rowDst = {0, 0, bufferClone->w, 1};
	for (int y = 0; y < bufferClone->h; ++y) {
		rowSrc.y = y;
		rowDst.y = bufferClone->h - 1 - y;
		SDL_BlitSurface(bufferClone, &rowSrc, target, &rowDst);
	}

	SDL_DestroySurface(bufferClone);
}
