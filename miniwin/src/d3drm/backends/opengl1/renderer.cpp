#include "actual.h"
#include "d3drmrenderer_opengl1.h"
#include "ddraw_impl.h"
#include "ddsurface_impl.h"
#include "mathutils.h"
#include "meshutils.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cstring>
#include <vector>

static_assert(sizeof(Matrix4x4) == sizeof(D3DRMMATRIX4D), "Matrix4x4 is wrong size");
static_assert(sizeof(GL11_BridgeVector) == sizeof(D3DVECTOR), "GL11_BridgeVector is wrong size");
static_assert(sizeof(GL11_BridgeTexCoord) == sizeof(TexCoord), "GL11_BridgeTexCoord is wrong size");
static_assert(sizeof(GL11_BridgeSceneLight) == sizeof(SceneLight), "GL11_BridgeSceneLight is wrong size");
static_assert(sizeof(GL11_BridgeSceneVertex) == sizeof(D3DRMVERTEX), "GL11_BridgeSceneVertex is wrong size");

Direct3DRMRenderer* OpenGL1Renderer::Create(DWORD width, DWORD height)
{
	// We have to reset the attributes here after having enumerated the
	// OpenGL ES 2.0 renderer, or else SDL gets very confused by SDL_GL_DEPTH_SIZE
	// call below when on an EGL-based backend, and crashes with EGL_BAD_MATCH.
	SDL_GL_ResetAttributes();
	// But ResetAttributes resets it to 16.
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

	SDL_Window* window = DDWindow;
	bool testWindow = false;
	if (!window) {
		window = SDL_CreateWindow("OpenGL 1.1 test", width, height, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
		if (!window) {
			SDL_Log("SDL_CreateWindow: %s", SDL_GetError());
			return nullptr;
		}
		testWindow = true;
	}

	SDL_GLContext context = SDL_GL_CreateContext(window);
	if (!context) {
		SDL_Log("SDL_GL_CreateContext: %s", SDL_GetError());
		if (testWindow) {
			SDL_DestroyWindow(window);
		}
		return nullptr;
	}

	if (!SDL_GL_MakeCurrent(window, context)) {
		SDL_Log("SDL_GL_MakeCurrent: %s", SDL_GetError());
		if (testWindow) {
			SDL_DestroyWindow(window);
		}
		return nullptr;
	}

	GL11_InitState();

	if (testWindow) {
		SDL_DestroyWindow(window);
	}

	return new OpenGL1Renderer(width, height, context);
}

OpenGL1Renderer::OpenGL1Renderer(DWORD width, DWORD height, SDL_GLContext context) : m_context(context)
{
	m_width = width;
	m_height = height;
	m_virtualWidth = width;
	m_virtualHeight = height;
	m_renderedImage = SDL_CreateSurface(m_width, m_height, SDL_PIXELFORMAT_RGBA32);
	GL11_LoadExtensions();
}

OpenGL1Renderer::~OpenGL1Renderer()
{
	SDL_DestroySurface(m_renderedImage);
}

void OpenGL1Renderer::PushLights(const SceneLight* lightsArray, size_t count)
{
	if (count > 8) {
		SDL_Log("Unsupported number of lights (%d)", static_cast<int>(count));
		count = 8;
	}

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
				GL11_DestroyTexture(cache.glTextureId);
				cache.glTextureId = 0;
				cache.texture = nullptr;
			}
			delete ctx;
		},
		ctx
	);
}

Uint32 OpenGL1Renderer::GetTextureId(IDirect3DRMTexture* iTexture, bool isUi)
{
	auto texture = static_cast<Direct3DRMTextureImpl*>(iTexture);
	auto surface = static_cast<DirectDrawSurfaceImpl*>(texture->m_surface);

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (tex.texture == texture) {
			if (tex.version != texture->m_version) {
				GL11_DestroyTexture(tex.glTextureId);

				SDL_Surface* surf = SDL_ConvertSurface(surface->m_surface, SDL_PIXELFORMAT_RGBA32);
				if (!surf) {
					return NO_TEXTURE_ID;
				}
				tex.glTextureId = GL11_UploadTextureData(surf->pixels, surf->w, surf->h);
				SDL_DestroySurface(surf);

				tex.version = texture->m_version;
			}
			return i;
		}
	}

	GLuint texId;

	SDL_Surface* surf = SDL_ConvertSurface(surface->m_surface, SDL_PIXELFORMAT_RGBA32);
	if (!surf) {
		return NO_TEXTURE_ID;
	}
	texId = GL11_UploadTextureData(surf->pixels, surf->w, surf->h);
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
			return GL11_BridgeTexCoord{v.texCoord.u, v.texCoord.v};
		});
	}
	cache.positions.resize(vertices.size());
	std::transform(vertices.begin(), vertices.end(), cache.positions.begin(), [](const D3DRMVERTEX& v) {
		return GL11_BridgeVector{v.position.x, v.position.y, v.position.z};
	});
	cache.normals.resize(vertices.size());
	std::transform(vertices.begin(), vertices.end(), cache.normals.begin(), [](const D3DRMVERTEX& v) {
		return GL11_BridgeVector{v.normal.x, v.normal.y, v.normal.z};
	});

	GL11_UploadMesh(cache, meshGroup.texture != nullptr);

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
			GL11_DestroyMesh(cache);
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

HRESULT OpenGL1Renderer::BeginFrame()
{
	GL11_BeginFrame((Matrix4x4*) &m_projection[0][0]);

	int lightIdx = 0;
	for (const auto& l : m_lights) {
		if (lightIdx > 7) {
			break;
		}
		GL11_UploadLight(lightIdx, (GL11_BridgeSceneLight*) &l);

		lightIdx++;
	}
	return DD_OK;
}

void OpenGL1Renderer::EnableTransparency()
{
	GL11_EnableTransparency();
}

void OpenGL1Renderer::SubmitDraw(
	DWORD meshId,
	const D3DRMMATRIX4D& modelViewMatrix,
	const D3DRMMATRIX4D& worldMatrix,
	const D3DRMMATRIX4D& viewMatrix,
	const Matrix3x3& normalMatrix,
	const Appearance& appearance
)
{
	auto& mesh = m_meshs[meshId];

	// Bind texture if present
	if (appearance.textureId != NO_TEXTURE_ID) {
		auto& tex = m_textures[appearance.textureId];
		GL11_SubmitDraw(mesh, modelViewMatrix, appearance, tex.glTextureId);
	}
	else {
		GL11_SubmitDraw(mesh, modelViewMatrix, appearance, 0);
	}
}

HRESULT OpenGL1Renderer::FinalizeFrame()
{
	return DD_OK;
}

void OpenGL1Renderer::Resize(int width, int height, const ViewportTransform& viewportTransform)
{
	m_width = width;
	m_height = height;
	m_viewportTransform = viewportTransform;
	SDL_DestroySurface(m_renderedImage);
	m_renderedImage = SDL_CreateSurface(m_width, m_height, SDL_PIXELFORMAT_RGBA32);
	GL11_Resize(width, height);
}

void OpenGL1Renderer::Clear(float r, float g, float b)
{
	m_dirty = true;
	GL11_Clear(r, g, b);
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

	float left = -m_viewportTransform.offsetX / m_viewportTransform.scale;
	float right = (m_width - m_viewportTransform.offsetX) / m_viewportTransform.scale;
	float top = -m_viewportTransform.offsetY / m_viewportTransform.scale;
	float bottom = (m_height - m_viewportTransform.offsetY) / m_viewportTransform.scale;

	GL11_Draw2DImage(m_textures[textureId].glTextureId, srcRect, dstRect, left, right, bottom, top);
}

void OpenGL1Renderer::Download(SDL_Surface* target)
{
	GL11_Download(m_renderedImage);

	SDL_Rect srcRect = {
		static_cast<int>(m_viewportTransform.offsetX),
		static_cast<int>(m_viewportTransform.offsetY),
		static_cast<int>(target->w * m_viewportTransform.scale),
		static_cast<int>(target->h * m_viewportTransform.scale),
	};

	SDL_Surface* bufferClone = SDL_CreateSurface(target->w, target->h, SDL_PIXELFORMAT_RGBA32);
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
