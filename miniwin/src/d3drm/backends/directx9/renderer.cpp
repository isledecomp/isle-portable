#include "d3drmrenderer_directx9.h"
#include "ddraw_impl.h"
#include "ddsurface_impl.h"
#include "mathutils.h"
#include "meshutils.h"
#include "structs.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cstring>
#include <vector>

static_assert(sizeof(Matrix4x4) == sizeof(D3DRMMATRIX4D), "Matrix4x4 is wrong size");
static_assert(sizeof(BridgeVector) == sizeof(D3DVECTOR), "BridgeVector is wrong size");
static_assert(sizeof(BridgeSceneLight) == sizeof(SceneLight), "BridgeSceneLight is wrong size");
static_assert(sizeof(BridgeSceneVertex) == sizeof(D3DRMVERTEX), "BridgeSceneVertex is wrong size");

Direct3DRMRenderer* DirectX9Renderer::Create(DWORD width, DWORD height)
{
	return new DirectX9Renderer(width, height);
}

DirectX9Renderer::DirectX9Renderer(DWORD width, DWORD height)
{
	m_width = width;
	m_height = height;
	m_virtualWidth = width;
	m_virtualHeight = height;
	Actual_Initialize(
		SDL_GetPointerProperty(SDL_GetWindowProperties(DDWindow), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL),
		width,
		height
	);
	m_renderedImage = SDL_CreateSurface(m_width, m_height, SDL_PIXELFORMAT_RGBA32);
}

DirectX9Renderer::~DirectX9Renderer()
{
	Actual_Shutdown();
}

void DirectX9Renderer::PushLights(const SceneLight* lightsArray, size_t count)
{
	Actual_PushLights((BridgeSceneLight*) lightsArray, count);
}

void DirectX9Renderer::SetFrustumPlanes(const Plane* frustumPlanes)
{
}

void DirectX9Renderer::SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back)
{
	Actual_SetProjection(&projection, front, back);
}

struct TextureDestroyContextDX9 {
	DirectX9Renderer* renderer;
	Uint32 textureId;
};

void DirectX9Renderer::AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture)
{
	auto* ctx = new TextureDestroyContextDX9{this, id};
	texture->AddDestroyCallback(
		[](IDirect3DRMObject* obj, void* arg) {
			auto* ctx = static_cast<TextureDestroyContextDX9*>(arg);
			auto& cache = ctx->renderer->m_textures[ctx->textureId];
			if (cache.dxTexture) {
				ReleaseD3DTexture(cache.dxTexture);
				cache.dxTexture = nullptr;
				cache.texture = nullptr;
			}
			delete ctx;
		},
		ctx
	);
}

Uint32 DirectX9Renderer::GetTextureId(IDirect3DRMTexture* iTexture, bool isUI, float scaleX, float scaleY)
{
	auto texture = static_cast<Direct3DRMTextureImpl*>(iTexture);
	auto surface = static_cast<DirectDrawSurfaceImpl*>(texture->m_surface);

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (tex.texture == texture) {
			if (tex.version != texture->m_version) {
				if (tex.dxTexture) {
					ReleaseD3DTexture(tex.dxTexture);
					tex.dxTexture = nullptr;
				}
				tex.dxTexture = UploadSurfaceToD3DTexture(surface->m_surface);
				if (!tex.dxTexture) {
					return NO_TEXTURE_ID;
				}
				tex.version = texture->m_version;
			}
			return i;
		}
	}

	IDirect3DTexture9* newTex = UploadSurfaceToD3DTexture(surface->m_surface);
	if (!newTex) {
		return NO_TEXTURE_ID;
	}

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (!tex.texture) {
			tex.texture = texture;
			tex.version = texture->m_version;
			tex.dxTexture = newTex;
			AddTextureDestroyCallback(i, texture);
			return i;
		}
	}

	m_textures.push_back({texture, texture->m_version, newTex});
	AddTextureDestroyCallback((Uint32) (m_textures.size() - 1), texture);
	return (Uint32) (m_textures.size() - 1);
}

D3D9MeshCacheEntry UploadD3D9Mesh(const MeshGroup& meshGroup)
{
	D3D9MeshCacheEntry cache;
	cache.meshGroup = &meshGroup;
	cache.version = meshGroup.version;
	cache.flat = meshGroup.quality == D3DRMRENDER_FLAT || meshGroup.quality == D3DRMRENDER_UNLITFLAT;

	std::vector<D3DRMVERTEX> vertices;
	std::vector<uint16_t> indices;

	if (cache.flat) {
		FlattenSurfaces(
			meshGroup.vertices.data(),
			meshGroup.vertices.size(),
			meshGroup.indices.data(),
			meshGroup.indices.size(),
			meshGroup.texture != nullptr,
			vertices,
			indices
		);
	}
	else {
		vertices = meshGroup.vertices;
		indices.resize(meshGroup.indices.size());
		std::transform(meshGroup.indices.begin(), meshGroup.indices.end(), indices.begin(), [](DWORD i) {
			return static_cast<uint16_t>(i);
		});
	}

	cache.indexCount = indices.size();
	cache.vertexCount = vertices.size();

	UploadMeshBuffers(
		vertices.data(),
		vertices.size() * sizeof(D3DRMVERTEX),
		indices.data(),
		indices.size() * sizeof(uint16_t),
		cache
	);

	return cache;
}

struct D3D9MeshDestroyContext {
	DirectX9Renderer* renderer;
	Uint32 id;
};

void DirectX9Renderer::AddMeshDestroyCallback(Uint32 id, IDirect3DRMMesh* mesh)
{
	auto* ctx = new D3D9MeshDestroyContext{this, id};
	mesh->AddDestroyCallback(
		[](IDirect3DRMObject*, void* arg) {
			auto* ctx = static_cast<D3D9MeshDestroyContext*>(arg);
			auto& cache = ctx->renderer->m_meshs[ctx->id];
			if (cache.vbo) {
				ReleaseD3DVertexBuffer(cache.vbo);
				cache.vbo = nullptr;
			}
			if (cache.ibo) {
				ReleaseD3DIndexBuffer(cache.ibo);
				cache.ibo = nullptr;
			}
			cache.meshGroup = nullptr;

			delete ctx;
		},
		ctx
	);
}

Uint32 DirectX9Renderer::GetMeshId(IDirect3DRMMesh* mesh, const MeshGroup* meshGroup)
{
	for (Uint32 i = 0; i < m_meshs.size(); ++i) {
		auto& cache = m_meshs[i];
		if (cache.meshGroup == meshGroup) {
			if (cache.version != meshGroup->version) {
				cache = UploadD3D9Mesh(*meshGroup);
			}
			return i;
		}
	}

	auto newCache = UploadD3D9Mesh(*meshGroup);

	for (Uint32 i = 0; i < m_meshs.size(); ++i) {
		if (!m_meshs[i].meshGroup) {
			m_meshs[i] = std::move(newCache);
			return i;
		}
	}

	m_meshs.push_back(std::move(newCache));
	return static_cast<Uint32>(m_meshs.size() - 1);
}

HRESULT DirectX9Renderer::BeginFrame()
{
	return Actual_BeginFrame();
}

void DirectX9Renderer::EnableTransparency()
{
	Actual_EnableTransparency();
}

void DirectX9Renderer::SubmitDraw(
	DWORD meshId,
	const D3DRMMATRIX4D& modelViewMatrix,
	const D3DRMMATRIX4D& worldMatrix,
	const D3DRMMATRIX4D& viewMatrix,
	const Matrix3x3& normalMatrix,
	const Appearance& appearance
)
{
	IDirect3DTexture9* texture = nullptr;
	if (appearance.textureId != NO_TEXTURE_ID) {
		texture = m_textures[appearance.textureId].dxTexture;
	}
	Actual_SubmitDraw(
		&m_meshs[meshId],
		&modelViewMatrix,
		&worldMatrix,
		&viewMatrix,
		&normalMatrix,
		&appearance,
		texture
	);
}

HRESULT DirectX9Renderer::FinalizeFrame()
{
	return DD_OK;
}

void DirectX9Renderer::Resize(int width, int height, const ViewportTransform& viewportTransform)
{
	m_width = width;
	m_height = height;
	m_viewportTransform = viewportTransform;
	Actual_Resize(width, height, viewportTransform);
}

void DirectX9Renderer::Clear(float r, float g, float b)
{
	Actual_Clear(r, g, b);
}

void DirectX9Renderer::Flip()
{
	Actual_Flip();
}

void DirectX9Renderer::Draw2DImage(Uint32 textureId, const SDL_Rect& srcRect, const SDL_Rect& dstRect, FColor color)
{
	Actual_Draw2DImage(m_textures[textureId].dxTexture, srcRect, dstRect, color);
}

void DirectX9Renderer::SetDither(bool dither)
{
}

void DirectX9Renderer::Download(SDL_Surface* target)
{
	Actual_Download(target);
}
