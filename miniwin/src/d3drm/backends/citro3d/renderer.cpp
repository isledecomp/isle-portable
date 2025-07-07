#include "d3drmrenderer.h"
#include "d3drmrenderer_citro3d.h"
#include "d3drmtexture_impl.h"
#include "ddraw_impl.h"
#include "meshutils.h"
#include "miniwin.h"
#include "vshader_shbin.h"

#include <cstring>

static bool g_rendering = false;

static DVLB_s* vshader_dvlb;
static shaderProgram_s program;
static int uLoc_projection;
static int uLoc_modelView;
static int uLoc_meshColor;
static int uLoc_lightVec;
static int uLoc_lightClr;
static int uLoc_shininess;

Citro3DRenderer::Citro3DRenderer(DWORD width, DWORD height)
{
	m_width = 320;
	m_height = 240;
	m_virtualWidth = width;
	m_virtualHeight = height;

	gfxSetScreenFormat(GFX_BOTTOM, GSP_BGR8_OES);
	consoleInit(GFX_TOP, nullptr);
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

	m_renderTarget = C3D_RenderTargetCreate(m_height, m_width, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetOutput(
		m_renderTarget,
		GFX_BOTTOM,
		GFX_LEFT,
		GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) |
			GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
			GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO)
	);

	vshader_dvlb = DVLB_ParseFile((u32*) vshader_shbin, vshader_shbin_size);
	shaderProgramInit(&program);
	shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
	C3D_BindProgram(&program);

	C3D_CullFace(GPU_CULL_FRONT_CCW);

	uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");
	uLoc_modelView = shaderInstanceGetUniformLocation(program.vertexShader, "modelView");
	uLoc_meshColor = shaderInstanceGetUniformLocation(program.vertexShader, "meshColor");
	uLoc_lightVec = shaderInstanceGetUniformLocation(program.vertexShader, "lightVec");
	uLoc_lightClr = shaderInstanceGetUniformLocation(program.vertexShader, "lightClr");
	uLoc_shininess = shaderInstanceGetUniformLocation(program.vertexShader, "shininess");

	C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
	AttrInfo_Init(attrInfo);
	AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=position
	AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 3); // v2=normal
	AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 2); // v1=texcoord
}

Citro3DRenderer::~Citro3DRenderer()
{
	shaderProgramFree(&program);
	DVLB_Free(vshader_dvlb);
	C3D_Fini();
}

void Citro3DRenderer::PushLights(const SceneLight* lights, size_t count)
{
	m_lights.assign(lights, lights + count);
}

void Citro3DRenderer::SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back)
{
	memcpy(&m_projection, projection, sizeof(D3DRMMATRIX4D));
}

void Citro3DRenderer::SetFrustumPlanes(const Plane* frustumPlanes)
{
}

struct Citro3DCacheDestroyContext {
	Citro3DRenderer* renderer;
	Uint32 id;
};

void Citro3DRenderer::AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture)
{
	auto* ctx = new Citro3DCacheDestroyContext{this, id};
	texture->AddDestroyCallback(
		[](IDirect3DRMObject* obj, void* arg) {
			auto* ctx = static_cast<Citro3DCacheDestroyContext*>(arg);
			auto& entry = ctx->renderer->m_textures[ctx->id];
			if (entry.texture) {
				C3D_TexDelete(&entry.c3dTex);
				entry.texture = nullptr;
			}
			delete ctx;
		},
		ctx
	);
}

static int NearestPowerOfTwoClamp(int val)
{
	static const int sizes[] = {8, 16, 32, 64, 128, 256, 512};
	for (int size : sizes) {
		if (val <= size) {
			return size;
		}
	}
	return 512;
}

static SDL_Surface* ConvertAndResizeSurface(SDL_Surface* original, bool isUI, float scaleX, float scaleY)
{
	if (!isUI) {
		return SDL_ConvertSurface(original, SDL_PIXELFORMAT_RGBA8888);
	}

	scaleX = std::min(scaleX, 1.0f);
	scaleY = std::min(scaleY, 1.0f);

	int scaledW = static_cast<int>(original->w * scaleX);
	int scaledH = static_cast<int>(original->h * scaleY);

	int paddedW = NearestPowerOfTwoClamp(scaledW);
	int paddedH = NearestPowerOfTwoClamp(scaledH);

	SDL_Surface* padded = SDL_CreateSurface(paddedW, paddedH, SDL_PIXELFORMAT_RGBA8888);
	if (!padded) {
		return nullptr;
	}

	if (scaleX == 1.0f && scaleY == 1.0f) {
		SDL_BlitSurface(original, nullptr, padded, nullptr);
	}
	else {
		SDL_ScaleMode scaleMode = (scaleX >= 1.0f && scaleY >= 1.0f) ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR;
		SDL_Rect dstRect = {0, 0, scaledW, scaledH};
		SDL_BlitSurfaceScaled(original, nullptr, padded, &dstRect, scaleMode);
	}

	return padded;
}

static void EncodeTextureLayout(const u8* src, u8* dst, int width, int height)
{
	const int tileSize = 8;
	const int bytesPerPixel = 4;

	int tilesPerRow = (width + tileSize - 1) / tileSize;

	static const uint8_t mortonLUT[64] = {0,  1,  4,  5,  16, 17, 20, 21, 2,  3,  6,  7,  18, 19, 22, 23,
										  8,  9,  12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26, 27, 30, 31,
										  32, 33, 36, 37, 48, 49, 52, 53, 34, 35, 38, 39, 50, 51, 54, 55,
										  40, 41, 44, 45, 56, 57, 60, 61, 42, 43, 46, 47, 58, 59, 62, 63};

	for (int tileY = 0; tileY < height; tileY += tileSize) {
		for (int tileX = 0; tileX < width; tileX += tileSize) {
			int tileIndex = (tileY / tileSize) * tilesPerRow + (tileX / tileSize);
			tileIndex *= tileSize * tileSize;

			for (int y = 0; y < tileSize; ++y) {
				for (int x = 0; x < tileSize; ++x) {
					int srcX = tileX + x;
					int srcY = tileY + y;

					if (srcX >= width || srcY >= height) {
						continue;
					}

					int morton = mortonLUT[y * tileSize + x];
					int dstIndex = (tileIndex + morton) * bytesPerPixel;
					int srcIndex = ((height - 1 - srcY) * width + srcX);

					*(u32*) &dst[dstIndex] = ((u32*) src)[srcIndex];
				}
			}
		}
	}
}

static bool ConvertAndUploadTexture(C3D_Tex* tex, SDL_Surface* originalSurface, bool isUI, float scaleX, float scaleY)
{
	SDL_Surface* resized = ConvertAndResizeSurface(originalSurface, isUI, scaleX, scaleY);
	if (!resized) {
		return false;
	}

	int width = resized->w;
	int height = resized->h;

	C3D_TexInitParams params = {};
	params.width = width;
	params.height = height;
	params.format = GPU_RGBA8;
	params.maxLevel = isUI ? 0 : 4;
	params.type = GPU_TEX_2D;
	if (!C3D_TexInitWithParams(tex, nullptr, params)) {
		if (resized != originalSurface) {
			SDL_DestroySurface(resized);
		}
		return false;
	}

	uint8_t* tiledData = (uint8_t*) malloc(width * height * 4);
	if (!tiledData) {
		if (resized != originalSurface) {
			SDL_DestroySurface(resized);
		}
		return false;
	}

	EncodeTextureLayout((const u8*) resized->pixels, tiledData, width, height);
	if (resized != originalSurface) {
		SDL_DestroySurface(resized);
	}

	C3D_TexUpload(tex, tiledData);
	free(tiledData);

	if (isUI) {
		C3D_TexSetFilter(tex, GPU_NEAREST, GPU_NEAREST);
		C3D_TexSetWrap(tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
	}
	else {
		C3D_TexSetFilter(tex, GPU_LINEAR, GPU_LINEAR);
		C3D_TexSetWrap(tex, GPU_REPEAT, GPU_REPEAT);
		C3D_TexSetFilterMipmap(tex, GPU_LINEAR);
		C3D_TexGenerateMipmap(tex, GPU_TEXFACE_2D);
	}

	return true;
}

Uint32 Citro3DRenderer::GetTextureId(IDirect3DRMTexture* iTexture, bool isUI, float scaleX, float scaleY)
{
	auto texture = static_cast<Direct3DRMTextureImpl*>(iTexture);
	auto surface = static_cast<DirectDrawSurfaceImpl*>(texture->m_surface);
	SDL_Surface* originalSurface = surface->m_surface;

	int originalW = originalSurface->w;
	int originalH = originalSurface->h;

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (tex.texture == texture) {
			if (tex.version != texture->m_version) {
				C3D_TexDelete(&tex.c3dTex);
				if (!ConvertAndUploadTexture(
						&tex.c3dTex,
						originalSurface,
						isUI,
						scaleX * m_viewportTransform.scale,
						scaleY * m_viewportTransform.scale
					)) {
					return NO_TEXTURE_ID;
				}

				tex.version = texture->m_version;
				tex.width = NearestPowerOfTwoClamp(originalW * m_viewportTransform.scale);
				tex.height = NearestPowerOfTwoClamp(originalH * m_viewportTransform.scale);
			}
			return i;
		}
	}

	C3DTextureCacheEntry entry;
	entry.texture = texture;
	entry.version = texture->m_version;
	entry.width = NearestPowerOfTwoClamp(originalW * m_viewportTransform.scale);
	entry.height = NearestPowerOfTwoClamp(originalH * m_viewportTransform.scale);

	if (!ConvertAndUploadTexture(
			&entry.c3dTex,
			originalSurface,
			isUI,
			scaleX * m_viewportTransform.scale,
			scaleY * m_viewportTransform.scale
		)) {
		return NO_TEXTURE_ID;
	}

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		if (!m_textures[i].texture) {
			m_textures[i] = std::move(entry);
			AddTextureDestroyCallback(i, texture);
			return i;
		}
	}

	m_textures.push_back(std::move(entry));
	AddTextureDestroyCallback((Uint32) (m_textures.size() - 1), texture);
	return (Uint32) (m_textures.size() - 1);
}

C3DMeshCacheEntry C3DUploadMesh(const MeshGroup& meshGroup)
{
	C3DMeshCacheEntry cache{&meshGroup, meshGroup.version};

	std::vector<D3DRMVERTEX> vertexBuffer;
	std::vector<uint16_t> indexBuffer;

	if (meshGroup.quality == D3DRMRENDER_FLAT || meshGroup.quality == D3DRMRENDER_UNLITFLAT) {
		FlattenSurfaces(
			meshGroup.vertices.data(),
			meshGroup.vertices.size(),
			meshGroup.indices.data(),
			meshGroup.indices.size(),
			meshGroup.texture != nullptr,
			vertexBuffer,
			indexBuffer
		);
	}
	else {
		vertexBuffer.assign(meshGroup.vertices.begin(), meshGroup.vertices.end());
		indexBuffer.assign(meshGroup.indices.begin(), meshGroup.indices.end());
	}

	// Flatten vertices as IBO is buggy on 3DS hardware
	std::vector<D3DRMVERTEX> vertexUploadBuffer;
	vertexUploadBuffer.reserve(indexBuffer.size());

	for (size_t i = 0; i < indexBuffer.size(); ++i) {
		vertexUploadBuffer.emplace_back(vertexBuffer[indexBuffer[i]]);
	}

	size_t vertexBufferSize = vertexUploadBuffer.size() * sizeof(D3DRMVERTEX);
	cache.vbo = linearAlloc(vertexBufferSize);
	memcpy(cache.vbo, vertexUploadBuffer.data(), vertexBufferSize);
	cache.vertexCount = vertexUploadBuffer.size();

	return cache;
}

void Citro3DRenderer::AddMeshDestroyCallback(Uint32 id, IDirect3DRMMesh* mesh)
{
	auto* ctx = new Citro3DCacheDestroyContext{this, id};
	mesh->AddDestroyCallback(
		[](IDirect3DRMObject* obj, void* arg) {
			auto* ctx = static_cast<Citro3DCacheDestroyContext*>(arg);
			auto& cacheEntry = ctx->renderer->m_meshs[ctx->id];
			if (cacheEntry.meshGroup) {
				cacheEntry.meshGroup = nullptr;
				linearFree(cacheEntry.vbo);
				cacheEntry.vertexCount = 0;
			}
			delete ctx;
		},
		ctx
	);
}

Uint32 Citro3DRenderer::GetMeshId(IDirect3DRMMesh* mesh, const MeshGroup* meshGroup)
{
	for (Uint32 i = 0; i < m_meshs.size(); ++i) {
		auto& cache = m_meshs[i];
		if (cache.meshGroup == meshGroup) {
			if (cache.version != meshGroup->version) {
				cache = std::move(C3DUploadMesh(*meshGroup));
			}
			return i;
		}
	}

	auto newCache = C3DUploadMesh(*meshGroup);

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

void Citro3DRenderer::StartFrame()
{
	if (g_rendering) {
		return;
	}
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	C3D_FrameDrawOn(m_renderTarget);
	g_rendering = true;
}

void ConvertPerspective(const D3DRMMATRIX4D in, C3D_Mtx* out)
{
	float f_h = in[0][0];
	float f_v = in[1][1];

	float aspect = f_v / f_h;
	float fovY = 2.0f * atanf(1.0f / f_v);

	float nearZ = -in[3][2] / in[2][2];
	float farZ = nearZ * in[2][2] / (in[2][2] - 1.0f);

	Mtx_PerspTilt(out, fovY, aspect, nearZ, farZ, true);
}

HRESULT Citro3DRenderer::BeginFrame()
{
	StartFrame();
	C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);

	C3D_Mtx projection;
	ConvertPerspective(m_projection, &projection);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);

	for (const auto& light : m_lights) {
		FColor lightColor = light.color;
		if (light.positional == 0.0f && light.directional == 0.0f) {
			// Ambient light
			C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_lightClr + 2, lightColor.r, lightColor.g, lightColor.b, 1.0f);
		}
		else if (light.directional == 1.0f) {
			C3D_FVUnifSet(
				GPU_VERTEX_SHADER,
				uLoc_lightVec + 1,
				-light.direction.x,
				-light.direction.y,
				-light.direction.z,
				0.0f
			);
			C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_lightClr + 1, lightColor.r, lightColor.g, lightColor.b, 0.0f);
		}
		else if (light.positional == 1.0f) {
			C3D_FVUnifSet(
				GPU_VERTEX_SHADER,
				uLoc_lightVec + 0,
				light.position.x,
				light.position.y,
				light.position.z,
				0.0f
			);
			C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_lightClr + 0, lightColor.r, lightColor.g, lightColor.b, 0.0f);
		}
	}

	return S_OK;
}

void Citro3DRenderer::EnableTransparency()
{
	C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_COLOR);
}

void ConvertMatrix(const D3DRMMATRIX4D in, C3D_Mtx* out)
{
	for (int i = 0; i < 4; i++) {
		out->r[i].x = in[0][i];
		out->r[i].y = in[1][i];
		out->r[i].z = in[2][i];
		out->r[i].w = in[3][i];
	}
}

void SetMaterialAppearance(
	const FColor& color,
	float shininess,
	int uLoc_meshColor,
	int uLoc_shininess,
	C3D_Tex* textures
)
{
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_meshColor, color.r, color.g, color.b, color.a);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_shininess, shininess, 0.0f, 0.0f, 0.0f);

	C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvInit(env);

	if (textures) {
		C3D_TexBind(0, textures);
		C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
		C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
	}
	else {
		C3D_TexBind(0, nullptr);
		C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
		C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
	}
}

void Citro3DRenderer::SubmitDraw(
	DWORD meshId,
	const D3DRMMATRIX4D& modelViewMatrix,
	const D3DRMMATRIX4D& worldMatrix,
	const D3DRMMATRIX4D& viewMatrix,
	const Matrix3x3& normalMatrix,
	const Appearance& appearance
)
{
	C3D_Mtx modelView;
	ConvertMatrix(modelViewMatrix, &modelView);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);

	auto& mesh = m_meshs[meshId];

	C3D_BufInfo* bufInfo = C3D_GetBufInfo();
	BufInfo_Init(bufInfo);
	BufInfo_Add(bufInfo, mesh.vbo, sizeof(D3DRMVERTEX), 3, 0x210);

	SetMaterialAppearance(
		{appearance.color.r / 255.0f,
		 appearance.color.g / 255.0f,
		 appearance.color.b / 255.0f,
		 appearance.color.a / 255.0f},
		appearance.shininess,
		uLoc_meshColor,
		uLoc_shininess,
		appearance.textureId != NO_TEXTURE_ID ? &m_textures[appearance.textureId].c3dTex : nullptr
	);

	C3D_DrawArrays(GPU_TRIANGLES, 0, mesh.vertexCount);
}

HRESULT Citro3DRenderer::FinalizeFrame()
{
	return S_OK;
}

void Citro3DRenderer::Resize(int width, int height, const ViewportTransform& viewportTransform)
{
	m_width = width;
	m_height = height;
	m_viewportTransform = viewportTransform;
}

void Citro3DRenderer::Clear(float r, float g, float b)
{
	StartFrame();
	u32 color =
		(static_cast<u32>(r * 255) << 24) | (static_cast<u32>(g * 255) << 16) | (static_cast<u32>(b * 255) << 8) | 255;
	C3D_RenderTargetClear(m_renderTarget, C3D_CLEAR_ALL, color, 0);
}

void Citro3DRenderer::Flip()
{
	C3D_FrameEnd(0);
	gfxFlushBuffers();
	gspWaitForVBlank();
	g_rendering = false;
}

void Citro3DRenderer::Draw2DImage(Uint32 textureId, const SDL_Rect& srcRect, const SDL_Rect& dstRect, FColor color)
{
	C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
	StartFrame();
	C3D_DepthTest(false, GPU_GREATER, GPU_WRITE_COLOR);

	float left = -m_viewportTransform.offsetX / m_viewportTransform.scale;
	float right = (m_width - m_viewportTransform.offsetX) / m_viewportTransform.scale;
	float top = -m_viewportTransform.offsetY / m_viewportTransform.scale;
	float bottom = (m_height - m_viewportTransform.offsetY) / m_viewportTransform.scale;

	C3D_Mtx projection, modelView;
	Mtx_OrthoTilt(&projection, left, right, bottom, top, 0.0f, 1.0f, true);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
	Mtx_Identity(&modelView);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);

	// Set light directions
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_lightVec + 0, 0.0f, 0.0f, 0.0f, 0.0f);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_lightVec + 1, 0.0f, 0.0f, 0.0f, 0.0f);

	// Set light colors
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_lightClr + 0, 0.0f, 0.0f, 0.0f, 0.0f);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_lightClr + 1, 0.0f, 0.0f, 0.0f, 0.0f);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_lightClr + 2, 1.0f, 1.0f, 1.0f, 1.0f); // Ambient

	C3DTextureCacheEntry* texture = (textureId != NO_TEXTURE_ID) ? &m_textures[textureId] : nullptr;

	SetMaterialAppearance(color, 0.0f, uLoc_meshColor, uLoc_shininess, texture ? &texture->c3dTex : nullptr);

	float scale = m_viewportTransform.scale;

	float x1 = static_cast<float>(dstRect.x);
	float y1 = static_cast<float>(dstRect.y);
	float x2 = x1 + static_cast<float>(dstRect.w);
	float y2 = y1 + static_cast<float>(dstRect.h);

	float u0 = 0.0f;
	float u1 = 0.0f;
	float v0 = 0.0f;
	float v1 = 0.0f;

	if (texture) {
		u0 = (srcRect.x * scale) / texture->width;
		u1 = ((srcRect.x + srcRect.w) * scale) / texture->width;
		v0 = (srcRect.y * scale) / texture->height;
		v1 = ((srcRect.y + srcRect.h) * scale) / texture->height;
	}

	C3D_ImmDrawBegin(GPU_TRIANGLES);

	// Triangle 1
	C3D_ImmSendAttrib(x1, y1, 0.5f, 0.0f);
	C3D_ImmSendAttrib(0.0f, 0.0f, 1.0f, 0.0f);
	C3D_ImmSendAttrib(u0, v0, 0.0f, 0.0f);

	C3D_ImmSendAttrib(x2, y1, 0.5f, 0.0f);
	C3D_ImmSendAttrib(0.0f, 0.0f, 1.0f, 0.0f);
	C3D_ImmSendAttrib(u1, v0, 0.0f, 0.0f);

	C3D_ImmSendAttrib(x2, y2, 0.5f, 0.0f);
	C3D_ImmSendAttrib(0.0f, 0.0f, 1.0f, 0.0f);
	C3D_ImmSendAttrib(u1, v1, 0.0f, 0.0f);

	// Triangle 2
	C3D_ImmSendAttrib(x2, y2, 0.5f, 0.0f);
	C3D_ImmSendAttrib(0.0f, 0.0f, 1.0f, 0.0f);
	C3D_ImmSendAttrib(u1, v1, 0.0f, 0.0f);

	C3D_ImmSendAttrib(x1, y2, 0.5f, 0.0f);
	C3D_ImmSendAttrib(0.0f, 0.0f, 1.0f, 0.0f);
	C3D_ImmSendAttrib(u0, v1, 0.0f, 0.0f);

	C3D_ImmSendAttrib(x1, y1, 0.5f, 0.0f);
	C3D_ImmSendAttrib(0.0f, 0.0f, 1.0f, 0.0f);
	C3D_ImmSendAttrib(u0, v0, 0.0f, 0.0f);

	C3D_ImmDrawEnd();
}

void Citro3DRenderer::SetDither(bool dither)
{
}

void Citro3DRenderer::Download(SDL_Surface* target)
{
	u16 width, height;
	u8* fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &width, &height);
	if (!fb) {
		SDL_Log("Failed to get framebuffer");
		return;
	}

	SDL_Surface* srcSurface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_BGR24, fb, width * 3);
	if (!srcSurface) {
		SDL_Log("SDL_CreateSurfaceFrom failed: %s", SDL_GetError());
		return;
	}

	SDL_Surface* convertedSurface = SDL_ConvertSurface(srcSurface, target->format);
	SDL_DestroySurface(srcSurface);
	if (!convertedSurface) {
		SDL_Log("SDL_ConvertSurface failed: %s", SDL_GetError());
		return;
	}

	int rotatedWidth = height;
	int rotatedHeight = width;
	SDL_Surface* rotatedSurface = SDL_CreateSurface(rotatedWidth, rotatedHeight, target->format);
	if (!rotatedSurface) {
		SDL_Log("SDL_CreateSurface failed: %s", SDL_GetError());
		SDL_DestroySurface(convertedSurface);
		return;
	}

	Uint32* srcPixels = (Uint32*) convertedSurface->pixels;
	Uint32* dstPixels = (Uint32*) rotatedSurface->pixels;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			Uint32 pixel = srcPixels[y * width + x];
			int newX = y;
			int newY = width - 1 - x;
			dstPixels[newY * rotatedWidth + newX] = pixel;
		}
	}

	SDL_DestroySurface(convertedSurface);

	SDL_Rect srcRect = {0, 0, rotatedSurface->w, rotatedSurface->h};
	SDL_Rect dstRect = {0, 0, target->w, target->h};
	SDL_BlitSurfaceScaled(rotatedSurface, &srcRect, target, &dstRect, SDL_SCALEMODE_NEAREST);

	SDL_DestroySurface(rotatedSurface);
}
