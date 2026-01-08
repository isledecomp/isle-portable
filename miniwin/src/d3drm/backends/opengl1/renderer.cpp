#include "actual.h"
#include "d3drmrenderer_opengl1.h"
#include "ddraw_impl.h"
#include "ddsurface_impl.h"
#include "mathutils.h"
#include "meshutils.h"

#include <algorithm>
#include <cstring>
#include <mortar/mortar.h>
#include <vector>

static_assert(sizeof(Matrix4x4) == sizeof(D3DRMMATRIX4D), "Matrix4x4 is wrong size");
static_assert(sizeof(GL11_BridgeVector) == sizeof(D3DVECTOR), "GL11_BridgeVector is wrong size");
static_assert(sizeof(GL11_BridgeTexCoord) == sizeof(TexCoord), "GL11_BridgeTexCoord is wrong size");
static_assert(sizeof(GL11_BridgeSceneLight) == sizeof(SceneLight), "GL11_BridgeSceneLight is wrong size");
static_assert(sizeof(GL11_BridgeSceneVertex) == sizeof(D3DRMVERTEX), "GL11_BridgeSceneVertex is wrong size");

Direct3DRMRenderer* OpenGL1Renderer::Create(DWORD width, DWORD height, DWORD msaaSamples)
{
	// We have to reset the attributes here after having enumerated the
	// OpenGL ES 2.0 renderer, or else SDL gets very confused by MORTAR_GL_DEPTH_SIZE
	// call below when on an EGL-based backend, and crashes with EGL_BAD_MATCH.
	MORTAR_GL_ResetAttributes();
	// But ResetAttributes resets it to 16.
	MORTAR_GL_SetAttribute(MORTAR_GL_DEPTH_SIZE, 24);
	MORTAR_GL_SetAttribute(MORTAR_GL_CONTEXT_PROFILE_MASK, MORTAR_GL_CONTEXT_PROFILE_COMPATIBILITY);
	MORTAR_GL_SetAttribute(MORTAR_GL_CONTEXT_MAJOR_VERSION, 1);
	MORTAR_GL_SetAttribute(MORTAR_GL_CONTEXT_MINOR_VERSION, 1);

	if (msaaSamples > 1) {
		MORTAR_GL_SetAttribute(MORTAR_GL_MULTISAMPLEBUFFERS, 1);
		MORTAR_GL_SetAttribute(MORTAR_GL_MULTISAMPLESAMPLES, msaaSamples);
	}

	if (!DDWindow) {
		MORTAR_Log("No window handler");
		return nullptr;
	}

	MORTAR_GLContext context = MORTAR_GL_CreateContext(DDWindow);
	if (!context) {
		MORTAR_Log("MORTAR_GL_CreateContext: %s", MORTAR_GetError());
		return nullptr;
	}

	if (!MORTAR_GL_MakeCurrent(DDWindow, context)) {
		MORTAR_GL_DestroyContext(context);
		MORTAR_Log("MORTAR_GL_MakeCurrent: %s", MORTAR_GetError());
		return nullptr;
	}

	GL11_InitState();

	return new OpenGL1Renderer(width, height, context);
}

OpenGL1Renderer::OpenGL1Renderer(DWORD width, DWORD height, MORTAR_GLContext context) : m_context(context)
{
	m_width = width;
	m_height = height;
	m_virtualWidth = width;
	m_virtualHeight = height;
	m_renderedImage = MORTAR_CreateSurface(m_width, m_height, MORTAR_PIXELFORMAT_RGBA32);
	GL11_LoadExtensions();
	m_useVBOs = MORTAR_GL_ExtensionSupported("GL_ARB_vertex_buffer_object");
	m_useNPOT = MORTAR_GL_ExtensionSupported("GL_OES_texture_npot");
}

OpenGL1Renderer::~OpenGL1Renderer()
{
	MORTAR_DestroySurface(m_renderedImage);
	MORTAR_GL_DestroyContext(m_context);
}

void OpenGL1Renderer::PushLights(const SceneLight* lightsArray, size_t count)
{
	if (count > 8) {
		MORTAR_Log("Unsupported number of lights (%d)", static_cast<int>(count));
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
	uint32_t textureId;
};

void OpenGL1Renderer::AddTextureDestroyCallback(uint32_t id, IDirect3DRMTexture* texture)
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

static int NextPowerOfTwo(int v)
{
	int power = 1;
	while (power < v) {
		power <<= 1;
	}
	return power;
}

static uint32_t UploadTextureData(MORTAR_Surface* src, bool useNPOT, bool isUI, float scaleX, float scaleY)
{
	MORTAR_Surface* working = src;
	if (src->format != MORTAR_PIXELFORMAT_RGBA32) {
		working = MORTAR_ConvertSurface(src, MORTAR_PIXELFORMAT_RGBA32);
		if (!working) {
			MORTAR_Log("MORTAR_ConvertSurface failed: %s", MORTAR_GetError());
			return NO_TEXTURE_ID;
		}
	}

	MORTAR_Surface* finalSurface = working;

	int newW = working->w;
	int newH = working->h;
	if (!useNPOT) {
		newW = NextPowerOfTwo(newW);
		newH = NextPowerOfTwo(newH);
	}
	int max = GL11_GetMaxTextureSize();
	if (newW > max) {
		newW = max;
	}
	if (newH > max) {
		newH = max;
	}

	if (newW != working->w || newH != working->h) {
		MORTAR_Surface* resized = MORTAR_CreateSurface(newW, newH, working->format);
		if (!resized) {
			MORTAR_Log("MORTAR_CreateSurface (resize) failed: %s", MORTAR_GetError());
			if (working != src) {
				MORTAR_DestroySurface(working);
			}
			return NO_TEXTURE_ID;
		}

		MORTAR_Rect srcRect = {0, 0, working->w, working->h};
		MORTAR_Rect dstRect = {0, 0, newW, newH};
		MORTAR_BlitSurfaceScaled(working, &srcRect, resized, &dstRect, MORTAR_SCALEMODE_NEAREST);

		if (working != src) {
			MORTAR_DestroySurface(working);
		}
		finalSurface = resized;
	}

	uint32_t texId =
		GL11_UploadTextureData(finalSurface->pixels, finalSurface->w, finalSurface->h, isUI, scaleX, scaleY);
	if (finalSurface != src) {
		MORTAR_DestroySurface(finalSurface);
	}
	return texId;
}

uint32_t OpenGL1Renderer::GetTextureId(IDirect3DRMTexture* iTexture, bool isUI, float scaleX, float scaleY)
{
	MORTAR_GL_MakeCurrent(DDWindow, m_context);
	auto texture = static_cast<Direct3DRMTextureImpl*>(iTexture);
	auto surface = static_cast<DirectDrawSurfaceImpl*>(texture->m_surface);

	for (uint32_t i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (tex.texture == texture) {
			if (tex.version != texture->m_version) {
				GL11_DestroyTexture(tex.glTextureId);
				tex.glTextureId = UploadTextureData(surface->m_surface, m_useNPOT, isUI, scaleX, scaleY);
				tex.version = texture->m_version;
				tex.width = surface->m_surface->w;
				tex.height = surface->m_surface->h;
			}
			return i;
		}
	}

	GLuint texId = UploadTextureData(surface->m_surface, m_useNPOT, isUI, scaleX, scaleY);

	for (uint32_t i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (!tex.texture) {
			tex.texture = texture;
			tex.version = texture->m_version;
			tex.glTextureId = texId;
			tex.width = surface->m_surface->w;
			tex.height = surface->m_surface->h;
			AddTextureDestroyCallback(i, texture);
			return i;
		}
	}

	m_textures.push_back(
		{texture,
		 texture->m_version,
		 texId,
		 static_cast<float>(surface->m_surface->w),
		 static_cast<float>(surface->m_surface->h)}
	);
	AddTextureDestroyCallback((uint32_t) (m_textures.size() - 1), texture);
	return (uint32_t) (m_textures.size() - 1);
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
	uint32_t id;
};

void OpenGL1Renderer::AddMeshDestroyCallback(uint32_t id, IDirect3DRMMesh* mesh)
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

uint32_t OpenGL1Renderer::GetMeshId(IDirect3DRMMesh* mesh, const MeshGroup* meshGroup)
{
	for (uint32_t i = 0; i < m_meshs.size(); ++i) {
		auto& cache = m_meshs[i];
		if (cache.meshGroup == meshGroup) {
			if (cache.version != meshGroup->version) {
				cache = std::move(GLUploadMesh(*meshGroup, m_useVBOs));
			}
			return i;
		}
	}

	auto newCache = GLUploadMesh(*meshGroup, m_useVBOs);

	for (uint32_t i = 0; i < m_meshs.size(); ++i) {
		auto& cache = m_meshs[i];
		if (!cache.meshGroup) {
			cache = std::move(newCache);
			AddMeshDestroyCallback(i, mesh);
			return i;
		}
	}

	m_meshs.push_back(std::move(newCache));
	AddMeshDestroyCallback((uint32_t) (m_meshs.size() - 1), mesh);
	return (uint32_t) (m_meshs.size() - 1);
}

HRESULT OpenGL1Renderer::BeginFrame()
{
	MORTAR_GL_MakeCurrent(DDWindow, m_context);
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
	MORTAR_GL_MakeCurrent(DDWindow, m_context);
	m_width = width;
	m_height = height;
	m_viewportTransform = viewportTransform;
	MORTAR_DestroySurface(m_renderedImage);
	m_renderedImage = MORTAR_CreateSurface(m_width, m_height, MORTAR_PIXELFORMAT_RGBA32);
	GL11_Resize(width, height);
}

void OpenGL1Renderer::Clear(float r, float g, float b)
{
	MORTAR_GL_MakeCurrent(DDWindow, m_context);
	m_dirty = true;
	GL11_Clear(r, g, b);
}

void OpenGL1Renderer::Flip()
{
	MORTAR_GL_MakeCurrent(DDWindow, m_context);
	if (m_dirty) {
		MORTAR_GL_SwapWindow(DDWindow);
		m_dirty = false;
	}
}

void OpenGL1Renderer::Draw2DImage(
	uint32_t textureId,
	const MORTAR_Rect& srcRect,
	const MORTAR_Rect& dstRect,
	FColor color
)
{
	MORTAR_GL_MakeCurrent(DDWindow, m_context);
	m_dirty = true;

	float left = -m_viewportTransform.offsetX / m_viewportTransform.scale;
	float right = (m_width - m_viewportTransform.offsetX) / m_viewportTransform.scale;
	float top = -m_viewportTransform.offsetY / m_viewportTransform.scale;
	float bottom = (m_height - m_viewportTransform.offsetY) / m_viewportTransform.scale;

	const GLTextureCacheEntry* texture = nullptr;
	if (textureId != NO_TEXTURE_ID) {
		texture = &m_textures[textureId];
	}

	GL11_Draw2DImage(texture, srcRect, dstRect, color, left, right, bottom, top);
}

void OpenGL1Renderer::SetDither(bool dither)
{
	GL11_SetDither(dither);
}

void OpenGL1Renderer::Download(MORTAR_Surface* target)
{
	GL11_Download(m_renderedImage);

	MORTAR_Rect srcRect = {
		static_cast<int>(m_viewportTransform.offsetX),
		static_cast<int>(m_viewportTransform.offsetY),
		static_cast<int>(target->w * m_viewportTransform.scale),
		static_cast<int>(target->h * m_viewportTransform.scale),
	};

	MORTAR_Surface* bufferClone = MORTAR_CreateSurface(target->w, target->h, MORTAR_PIXELFORMAT_RGBA32);
	if (!bufferClone) {
		MORTAR_Log("MORTAR_CreateSurface: %s", MORTAR_GetError());
		return;
	}

	MORTAR_BlitSurfaceScaled(m_renderedImage, &srcRect, bufferClone, nullptr, MORTAR_SCALEMODE_NEAREST);

	// Flip image vertically into target
	MORTAR_Rect rowSrc = {0, 0, bufferClone->w, 1};
	MORTAR_Rect rowDst = {0, 0, bufferClone->w, 1};
	for (int y = 0; y < bufferClone->h; ++y) {
		rowSrc.y = y;
		rowDst.y = bufferClone->h - 1 - y;
		MORTAR_BlitSurface(bufferClone, &rowSrc, target, &rowDst);
	}

	MORTAR_DestroySurface(bufferClone);
}
