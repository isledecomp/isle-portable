#include "SDL3/SDL_surface.h"
#include "d3drmrenderer_citro3d.h"
#include "miniwin.h"
#include "miniwin/d3d.h"
#include "miniwin/d3drm.h"
#include "miniwin/windows.h"
#include "vshader_shbin.h"

#include <3ds/console.h>
#include <3ds/gfx.h>
#include <3ds/gpu/enums.h>
#include <3ds/gpu/gx.h>
#include <3ds/gpu/shaderProgram.h>
#include <c3d/framebuffer.h>
#include <c3d/renderqueue.h>
#include <c3d/texture.h>
#include <citro3d.h>
#include <cstring>
#include <tex3ds.h>

bool g_rendering = false;

#define DISPLAY_TRANSFER_FLAGS                                                                                         \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) |                                   \
	 GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |                     \
	 GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

static DVLB_s* vshader_dvlb;
static shaderProgram_s program;
static int uLoc_projection;
static int uLoc_modelView;
static int uLoc_meshColor;
C3D_RenderTarget* target;

typedef struct {
	float position[3];
	float texcoord[2];
	float normal[3];
} vertex;

static void* vbo_data_pos = nullptr;

static const vertex position_list[] = {
	{{200.0f, 200.0f, 0.5f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
	{{100.0f, 40.0f, 0.5f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
	{{300.0f, 40.0f, 0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
};
int g_vertexCount = 0;
_Static_assert(sizeof(vbo_data_pos) % 4 == 0, "vertex size not 4-byte aligned");

Citro3DRenderer::Citro3DRenderer(DWORD width, DWORD height)
{
	m_width = 400;
	m_height = 240;
	m_virtualWidth = width;
	m_virtualHeight = height;

	gfxInitDefault();
	consoleInit(GFX_BOTTOM, nullptr);
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

	target = C3D_RenderTargetCreate(m_height, m_width, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetOutput(target, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

	vshader_dvlb = DVLB_ParseFile((u32*) vshader_shbin, vshader_shbin_size);
	shaderProgramInit(&program);
	shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
	C3D_BindProgram(&program);

	uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");
	uLoc_modelView = shaderInstanceGetUniformLocation(program.vertexShader, "modelView");
	uLoc_meshColor = shaderInstanceGetUniformLocation(program.vertexShader, "meshColor");

	C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
	AttrInfo_Init(attrInfo);
	AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=position
	AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // v1=texcoord
	AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 3); // v2=normal
}

Citro3DRenderer::~Citro3DRenderer()
{
	linearFree(vbo_data_pos);
	shaderProgramFree(&program);
	DVLB_Free(vshader_dvlb);
	C3D_Fini();
	gfxExit();
}

void Citro3DRenderer::PushLights(const SceneLight* lightsArray, size_t count)
{
}

void Citro3DRenderer::SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back)
{
	memcpy(&m_projection, projection, sizeof(D3DRMMATRIX4D));
}

void Citro3DRenderer::SetFrustumPlanes(const Plane* frustumPlanes)
{
}

struct TextureDestroyContextCitro {
	Citro3DRenderer* renderer;
	Uint32 textureId;
};

void Citro3DRenderer::AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture)
{
	auto* ctx = new TextureDestroyContextCitro{this, id};
	texture->AddDestroyCallback(
		[](IDirect3DRMObject* obj, void* arg) {
			auto* ctx = static_cast<TextureDestroyContextCitro*>(arg);
			auto& entry = ctx->renderer->m_textures[ctx->textureId];
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
	static const int sizes[] = {512, 256, 128, 64, 32, 16, 8};
	for (int size : sizes) {
		if (val >= size) {
			return size;
		}
	}
	return 8;
}

static SDL_Surface* ConvertAndResizeSurface(SDL_Surface* original)
{
	SDL_Surface* converted = SDL_ConvertSurface(original, SDL_PIXELFORMAT_RGBA8888);
	if (!converted) {
		return nullptr;
	}

	int newW = NearestPowerOfTwoClamp(converted->w);
	int newH = NearestPowerOfTwoClamp(converted->h);

	if (converted->w == newW && converted->h == newH) {
		return converted;
	}

	SDL_Surface* resized = SDL_CreateSurface(newW, newH, SDL_PIXELFORMAT_RGBA8888);
	if (!resized) {
		SDL_DestroySurface(converted);
		return nullptr;
	}

	SDL_BlitSurfaceScaled(converted, nullptr, resized, nullptr, SDL_SCALEMODE_NEAREST);

	SDL_DestroySurface(converted);
	return resized;
}

inline int mortonInterleave(int x, int y)
{
	int answer = 0;
	for (int i = 0; i < 3; ++i) {
		answer |= ((y >> i) & 1) << (2 * i + 1);
		answer |= ((x >> i) & 1) << (2 * i);
	}
	return answer;
}

static void EncodeTextureLayout(const u8* src, u8* dst, int width, int height)
{
	const int tileSize = 8;
	const int bytesPerPixel = 4;

	int tilesPerRow = (width + tileSize - 1) / tileSize;

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

					int mortonIndex = mortonInterleave(x, y);
					int dstIndex = (tileIndex + mortonIndex) * bytesPerPixel;
					int srcIndex = (srcY * width + srcX) * bytesPerPixel;

					std::memcpy(&dst[dstIndex], &src[srcIndex], bytesPerPixel);
				}
			}
		}
	}
}

static bool ConvertAndUploadTexture(C3D_Tex* tex, SDL_Surface* originalSurface)
{
	SDL_Surface* resized = ConvertAndResizeSurface(originalSurface);
	if (!resized) {
		return false;
	}

	int width = resized->w;
	int height = resized->h;

	if (!C3D_TexInit(tex, width, height, GPU_RGBA8)) {
		SDL_DestroySurface(resized);
		return false;
	}

	// Allocate buffer for tiled texture
	uint8_t* tiledData = (uint8_t*) malloc(width * height * 4);
	if (!tiledData) {
		SDL_DestroySurface(resized);
		return false;
	}

	EncodeTextureLayout((const u8*) resized->pixels, tiledData, width, height);

	C3D_TexUpload(tex, tiledData);
	C3D_TexSetFilter(tex, GPU_LINEAR, GPU_NEAREST);

	free(tiledData);
	SDL_DestroySurface(resized);
	return true;
}

Uint32 Citro3DRenderer::GetTextureId(IDirect3DRMTexture* iTexture)
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
				if (!ConvertAndUploadTexture(&tex.c3dTex, originalSurface)) {
					return NO_TEXTURE_ID;
				}

				tex.version = texture->m_version;
				tex.width = originalW;
				tex.height = originalH;
			}
			return i;
		}
	}

	C3DTextureCacheEntry entry;
	entry.texture = texture;
	entry.version = texture->m_version;
	entry.width = originalW;
	entry.height = originalH;

	if (!ConvertAndUploadTexture(&entry.c3dTex, originalSurface)) {
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

Uint32 Citro3DRenderer::GetMeshId(IDirect3DRMMesh* mesh, const MeshGroup* meshGroup)
{
	if (vbo_data_pos) {
		linearFree(vbo_data_pos);
	}

	const auto& verts = meshGroup->vertices;
	const auto& indices = meshGroup->indices;

	std::vector<vertex> vertices;
	vertices.reserve(indices.size());

	for (size_t i = 0; i < indices.size(); ++i) {
		const D3DRMVERTEX& src = verts[indices[i]];
		vertex dst;

		dst.position[0] = src.position.x;
		dst.position[1] = src.position.y;
		dst.position[2] = src.position.z;

		dst.normal[0] = src.normal.x;
		dst.normal[1] = src.normal.y;
		dst.normal[2] = src.normal.z;

		dst.texcoord[0] = src.tu;
		dst.texcoord[1] = src.tv;

		vertices.push_back(dst);
	}
	g_vertexCount = indices.size();

	vbo_data_pos = linearAlloc(sizeof(position_list));
	memcpy(vbo_data_pos, position_list, sizeof(position_list));
	g_vertexCount = 3;

	return 0;
}

void Citro3DRenderer::GetDesc(D3DDEVICEDESC* halDesc, D3DDEVICEDESC* helDesc)
{
}

const char* Citro3DRenderer::GetName()
{
	return "Citro3D";
}

void StartFrame()
{
	if (g_rendering) {
		return;
	}
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	C3D_FrameDrawOn(target);
}

HRESULT Citro3DRenderer::BeginFrame()
{
	StartFrame();
	// C3D_DepthTest(false, GPU_GREATER, GPU_WRITE_COLOR);
	return S_OK;
}

void Citro3DRenderer::EnableTransparency()
{
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
	C3D_Mtx projection, modelView;
	Mtx_OrthoTilt(&projection, 0.0, 400.0, 0.0, 240.0, 0.0, 1.0, true);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
	Mtx_Identity(&modelView);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);

	C3D_BufInfo* bufInfo = C3D_GetBufInfo();
	BufInfo_Init(bufInfo);
	BufInfo_Add(bufInfo, vbo_data_pos, sizeof(vertex), 3, 0x210);

	C3D_FVUnifSet(
		GPU_VERTEX_SHADER,
		uLoc_meshColor,
		appearance.color.r / 255.0f,
		appearance.color.g / 255.0f,
		appearance.color.b / 255.0f,
		appearance.color.a / 255.0f
	);

	if (appearance.textureId != NO_TEXTURE_ID) {
		C3D_TexBind(0, &m_textures[appearance.textureId].c3dTex);
		C3D_TexEnv* env = C3D_GetTexEnv(0);
		C3D_TexEnvInit(env);
		C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
		C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
	}
	else {
		C3D_TexBind(0, nullptr);
	}

	C3D_DrawArrays(GPU_TRIANGLES, 0, g_vertexCount);
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
	u32 color =
		(static_cast<u32>(r * 255) << 24) | (static_cast<u32>(g * 255) << 16) | (static_cast<u32>(b * 255) << 8) | 255;
	C3D_RenderTargetClear(target, C3D_CLEAR_ALL, color, 0);
}

void Citro3DRenderer::Flip()
{
	C3D_FrameEnd(0);
}

void Citro3DRenderer::Draw2DImage(Uint32 textureId, const SDL_Rect& srcRect, const SDL_Rect& dstRect)
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

	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_meshColor, 1.0f, 1.0f, 1.0f, 1.0f);

	C3DTextureCacheEntry& texture = m_textures[textureId];

	C3D_TexBind(0, &texture.c3dTex);
	C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
	C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);

	// Use dstRect size directly for quad size
	float quadW = static_cast<float>(dstRect.w);
	float quadH = static_cast<float>(dstRect.h);

	float x1 = static_cast<float>(dstRect.x);
	float y1 = static_cast<float>(dstRect.y);
	float x2 = x1 + quadW;
	float y2 = y1 + quadH;

	float u0 = static_cast<float>(srcRect.x) / texture.width;
	float u1 = static_cast<float>(srcRect.x + srcRect.w) / texture.width;
	float v1 = 1.0f - static_cast<float>(srcRect.y + srcRect.h) / texture.height;
	float v0 = 1.0f - static_cast<float>(srcRect.y) / texture.height;

	C3D_ImmDrawBegin(GPU_TRIANGLES);

	// Triangle 1
	C3D_ImmSendAttrib(x1, y1, 0.5f, 0.0f);
	C3D_ImmSendAttrib(u0, v0, 0.0f, 0.0f);
	C3D_ImmSendAttrib(0.0f, 0.0f, 1.0f, 0.0f);

	C3D_ImmSendAttrib(x2, y2, 0.5f, 0.0f);
	C3D_ImmSendAttrib(u1, v1, 0.0f, 0.0f);
	C3D_ImmSendAttrib(0.0f, 0.0f, 1.0f, 0.0f);

	C3D_ImmSendAttrib(x2, y1, 0.5f, 0.0f);
	C3D_ImmSendAttrib(u1, v0, 0.0f, 0.0f);
	C3D_ImmSendAttrib(0.0f, 0.0f, 1.0f, 0.0f);

	// Triangle 2
	C3D_ImmSendAttrib(x2, y2, 0.5f, 0.0f);
	C3D_ImmSendAttrib(u1, v1, 0.0f, 0.0f);
	C3D_ImmSendAttrib(0.0f, 0.0f, 1.0f, 0.0f);

	C3D_ImmSendAttrib(x1, y1, 0.5f, 0.0f);
	C3D_ImmSendAttrib(u0, v0, 0.0f, 0.0f);
	C3D_ImmSendAttrib(0.0f, 0.0f, 1.0f, 0.0f);

	C3D_ImmSendAttrib(x1, y2, 0.5f, 0.0f);
	C3D_ImmSendAttrib(u0, v1, 0.0f, 0.0f);
	C3D_ImmSendAttrib(0.0f, 0.0f, 1.0f, 0.0f);

	C3D_ImmDrawEnd();
}

void Citro3DRenderer::Download(SDL_Surface* target)
{
}
