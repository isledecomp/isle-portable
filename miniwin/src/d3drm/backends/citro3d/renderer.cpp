#include "SDL3/SDL_surface.h"
#include "d3drmrenderer_citro3d.h"
#include "miniwin.h"
#include "miniwin/d3d.h"
#include "miniwin/d3drm.h"
#include "miniwin/windows.h"

#include <3ds/console.h>
#include <3ds/gfx.h>
#include <3ds/gpu/enums.h>
#include <3ds/gpu/gx.h>
#include <3ds/gpu/shaderProgram.h>
#include <c3d/framebuffer.h>
#include <c3d/renderqueue.h>
#include <c3d/texture.h>
#include <3ds.h>
#include <citro3d.h>
#include <tex3ds.h>

#include "vshader_shbin.h"

int projectionShaderUniformLocation, modelViewUniformLocation;

typedef struct {
	float positions[3];
	float texcoords[2];
	float normals[3];
} Vertex;

// from this wiki: https://github.com/tommai78101/homebrew/wiki/Version-002:-Core-Engine
static const Vertex vertexList[] =
{
	// First face (PZ)
	// First triangle
	{ {-0.5f, -0.5f, +0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f, +1.0f} },
	{ {+0.5f, -0.5f, +0.5f}, {1.0f, 0.0f}, {0.0f, 0.0f, +1.0f} },
	{ {+0.5f, +0.5f, +0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, +1.0f} },
	// Second triangle
	{ {+0.5f, +0.5f, +0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, +1.0f} },
	{ {-0.5f, +0.5f, +0.5f}, {0.0f, 1.0f}, {0.0f, 0.0f, +1.0f} },
	{ {-0.5f, -0.5f, +0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f, +1.0f} },

	// Second face (MZ)
	// First triangle
	{ {-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} },
	{ {-0.5f, +0.5f, -0.5f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f} },
	{ {+0.5f, +0.5f, -0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f} },
	// Second triangle
	{ {+0.5f, +0.5f, -0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f} },
	{ {+0.5f, -0.5f, -0.5f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f} },
	{ {-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} },

	// Third face (PX)
	// First triangle
	{ {+0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {+1.0f, 0.0f, 0.0f} },
	{ {+0.5f, +0.5f, -0.5f}, {1.0f, 0.0f}, {+1.0f, 0.0f, 0.0f} },
	{ {+0.5f, +0.5f, +0.5f}, {1.0f, 1.0f}, {+1.0f, 0.0f, 0.0f} },
	// Second triangle
	{ {+0.5f, +0.5f, +0.5f}, {1.0f, 1.0f}, {+1.0f, 0.0f, 0.0f} },
	{ {+0.5f, -0.5f, +0.5f}, {0.0f, 1.0f}, {+1.0f, 0.0f, 0.0f} },
	{ {+0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {+1.0f, 0.0f, 0.0f} },

	// Fourth face (MX)
	// First triangle
	{ {-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f} },
	{ {-0.5f, -0.5f, +0.5f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f} },
	{ {-0.5f, +0.5f, +0.5f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f} },
	// Second triangle
	{ {-0.5f, +0.5f, +0.5f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f} },
	{ {-0.5f, +0.5f, -0.5f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f} },
	{ {-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f} },

	// Fifth face (PY)
	// First triangle
	{ {-0.5f, +0.5f, -0.5f}, {0.0f, 0.0f}, {0.0f, +1.0f, 0.0f} },
	{ {-0.5f, +0.5f, +0.5f}, {1.0f, 0.0f}, {0.0f, +1.0f, 0.0f} },
	{ {+0.5f, +0.5f, +0.5f}, {1.0f, 1.0f}, {0.0f, +1.0f, 0.0f} },
	// Second triangle
	{ {+0.5f, +0.5f, +0.5f}, {1.0f, 1.0f}, {0.0f, +1.0f, 0.0f} },
	{ {+0.5f, +0.5f, -0.5f}, {0.0f, 1.0f}, {0.0f, +1.0f, 0.0f} },
	{ {-0.5f, +0.5f, -0.5f}, {0.0f, 0.0f}, {0.0f, +1.0f, 0.0f} },

	// Sixth face (MY)
	// First triangle
	{ {-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f} },
	{ {+0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f} },
	{ {+0.5f, -0.5f, +0.5f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f} },
	// Second triangle
	{ {+0.5f, -0.5f, +0.5f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f} },
	{ {-0.5f, -0.5f, +0.5f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f} },
	{ {-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f} },
};

void *vbo_data;
void sceneInit(shaderProgram_s* prog) {
	MINIWIN_TRACE("set uniform loc");
	projectionShaderUniformLocation = shaderInstanceGetUniformLocation(prog->vertexShader, "projection");
	modelViewUniformLocation = shaderInstanceGetUniformLocation(prog->vertexShader, "modelView");

	// src: https://github.com/devkitPro/citro3d/blob/9f21cf7b380ce6f9e01a0420f19f0763e5443ca7/test/3ds/source/main.cpp#L122C3-L126C62
	MINIWIN_TRACE("pre attr info");
	C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
  	AttrInfo_Init(attrInfo);
  	AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=position
  	AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // v1=texcoord
  	AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 3); // v2=normal

   MINIWIN_TRACE("pre alloc");
   	vbo_data = linearAlloc(sizeof(vertexList));
    memcpy(vbo_data, vertexList, sizeof(vertexList));

    //Initialize and configure buffers.
    MINIWIN_TRACE("pre buf");
	C3D_BufInfo* bufferInfo = C3D_GetBufInfo();
	BufInfo_Init(bufferInfo);
	BufInfo_Add(bufferInfo, vbo_data, sizeof(Vertex), 3, 0x210);

	// this is probably wrong
	MINIWIN_TRACE("pre tex");
	C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
	C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
}

Direct3DRMRenderer* Citro3DRenderer::Create(DWORD width, DWORD height)
{
	// TODO: Doesn't SDL call this function?
	// Actually it's in ctrulib -max
	gfxInitDefault();
	gfxSetWide(true);
	gfxSet3D(false);
	consoleInit(GFX_BOTTOM, nullptr);

	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

	return new Citro3DRenderer(width, height);
}

// constructor parameters not finalized
Citro3DRenderer::Citro3DRenderer(DWORD width, DWORD height)
{
	static shaderProgram_s program;
	DVLB_s *vsh_dvlb;

	m_width = width;
	m_height = height / 2;
	m_virtualWidth = width;
	m_virtualHeight = height / 2;

	// FIXME: is this the right pixel format?

	shaderProgramInit(&program);
	vsh_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
	shaderProgramSetVsh(&program, &vsh_dvlb->DVLE[0]);

	// WARNING: This might crash, not sure
	SDL_Log("pre bind");
	C3D_BindProgram(&program);

	// todo: move to scene init next
	SDL_Log("setting uniform loc");
	sceneInit(&program);

	// TODO: is GPU_RB_RGBA8 correct?
	// TODO: is GPU_RB_DEPTH24_STENCIL8 correct?
	m_renderTarget = C3D_RenderTargetCreate(width, height, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);

	// TODO: what color should be used, if we shouldn't use 0x777777FF
	C3D_RenderTargetClear(m_renderTarget, C3D_CLEAR_ALL, 0x777777FF, 0);

	// TODO: Cleanup as we see what is needed
	m_flipVertFlag = 0;
	m_outTiledFlag = 0;
	m_rawCopyFlag = 0;

	// TODO: correct values?
	m_transferInputFormatFlag = GX_TRANSFER_FMT_RGBA8;
	m_transferOutputFormatFlag = GX_TRANSFER_FMT_RGB8;

	m_transferScaleFlag = GX_TRANSFER_SCALE_NO;

	m_transferFlags = (GX_TRANSFER_FLIP_VERT(m_flipVertFlag) | GX_TRANSFER_OUT_TILED(m_outTiledFlag) | \
		GX_TRANSFER_RAW_COPY(m_rawCopyFlag) | GX_TRANSFER_IN_FORMAT(m_transferInputFormatFlag) | \
		GX_TRANSFER_OUT_FORMAT(m_transferOutputFormatFlag) | GX_TRANSFER_SCALING(m_transferScaleFlag));

	C3D_RenderTargetSetOutput(m_renderTarget, GFX_TOP, GFX_LEFT, m_transferFlags);

	m_renderedImage = SDL_CreateSurface(m_width, m_height, SDL_PIXELFORMAT_RGBA32);
	MINIWIN_NOT_IMPLEMENTED();
}

Citro3DRenderer::~Citro3DRenderer()
{
	SDL_DestroySurface(m_renderedImage);
	C3D_RenderTargetDelete(m_renderTarget);
}

void Citro3DRenderer::PushLights(const SceneLight* lightsArray, size_t count)
{
	MINIWIN_NOT_IMPLEMENTED();
}

void Citro3DRenderer::SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back)
{
	MINIWIN_TRACE("Set projection");
	memcpy(&m_projection, projection, sizeof(D3DRMMATRIX4D));
}

void Citro3DRenderer::SetFrustumPlanes(const Plane* frustumPlanes)
{
	MINIWIN_NOT_IMPLEMENTED();
}

struct TextureDestroyContextC3D {
	Citro3DRenderer* renderer;
	Uint32 textureId;
};

void Citro3DRenderer::AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture)
{
	auto* ctx = new TextureDestroyContextC3D{this, id};
	texture->AddDestroyCallback(
		[](IDirect3DRMObject* obj, void* arg) {
			auto* ctx = static_cast<TextureDestroyContextC3D*>(arg);
			auto& cache = ctx->renderer->m_textures[ctx->textureId];
			if (cache.c3dTex != nullptr) {
				C3D_TexDelete(cache.c3dTex);
				cache.c3dTex = nullptr;
				cache.texture = nullptr;
			}
			delete ctx;
		},
		ctx
	);
}

Uint32 Citro3DRenderer::GetTextureId(IDirect3DRMTexture* iTexture)
{
	auto texture = static_cast<Direct3DRMTextureImpl*>(iTexture);
	auto surface = static_cast<DirectDrawSurfaceImpl*>(texture->m_surface);

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (tex.texture == texture) {
			if (tex.version != texture->m_version) {
				C3D_TexDelete(tex.c3dTex);

				SDL_Surface* surf = SDL_ConvertSurface(surface->m_surface, SDL_PIXELFORMAT_RGBA32);
				if (!surf) {
					return NO_TEXTURE_ID;
				}
				// Apparently a crash may be caused due to large textures? Hopefully this fixes that.
				surf = SDL_ScaleSurface(surf, (surf->w / 2), (surf->h / 2), SDL_SCALEMODE_LINEAR);

				// TODO: C3D_TexGenerateMipmap or C3D_TexInit?
				// glGenTextures(1, &tex.glTextureId);
				// FIXME: GPU_RGBA8 may be wrong
				C3D_TexInitMipmap(tex.c3dTex, surf->w, surf->h, GPU_RGBA8);
				Tex3DS_Texture t3x = Tex3DS_TextureImport(t3x, (size_t)(surf->w * surf->h), tex.c3dTex, NULL, false); 
				Tex3DS_TextureFree(t3x);

				C3D_TexBind(0, tex.c3dTex);
				C3D_TexUpload(tex.c3dTex, surf->pixels);
				SDL_DestroySurface(surf);

				tex.version = texture->m_version;
			}
			return i;
		}
	}

	C3D_Tex newTex;

	SDL_Surface* surf = SDL_ConvertSurface(surface->m_surface, SDL_PIXELFORMAT_RGBA32);
	if (!surf) {
		return NO_TEXTURE_ID;
	}
	C3D_TexInit(&newTex, surf->w, surf->h, GPU_RGBA8);
	C3D_TexBind(0, &newTex);
	C3D_TexUpload(&newTex, surf->pixels);
	SDL_DestroySurface(surf);

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (!tex.texture) {
			tex.texture = texture;
			tex.version = texture->m_version;
			tex.c3dTex = &newTex;
			AddTextureDestroyCallback(i, texture);
			return i;
		}
	}

	m_textures.push_back({texture, texture->m_version, &newTex});
	AddTextureDestroyCallback((Uint32) (m_textures.size() - 1), texture);
	return (Uint32) (m_textures.size() - 1);
}

Uint32 Citro3DRenderer::GetMeshId(IDirect3DRMMesh* mesh, const MeshGroup* meshGroup)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

void Citro3DRenderer::GetDesc(D3DDEVICEDESC* halDesc, D3DDEVICEDESC* helDesc)
{
	// not sure if this is correct?
	MINIWIN_NOT_IMPLEMENTED();

	halDesc->dcmColorModel = D3DCOLORMODEL::RGB;
	helDesc->dwFlags = D3DDD_DEVICEZBUFFERBITDEPTH;
	helDesc->dwDeviceZBufferBitDepth = DDBD_24;
	helDesc->dwDeviceRenderBitDepth = DDBD_24;
	helDesc->dpcTriCaps.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE;
	helDesc->dpcTriCaps.dwShadeCaps = D3DPSHADECAPS_ALPHAFLATBLEND;

	// TODO: shouldn't this be bilinear
	helDesc->dpcTriCaps.dwTextureFilterCaps = D3DPTFILTERCAPS_LINEAR;

	memset(helDesc, 0, sizeof(D3DDEVICEDESC));
}

const char* Citro3DRenderer::GetName()
{
	return "Citro3D";
}

HRESULT Citro3DRenderer::BeginFrame()
{
	MINIWIN_NOT_IMPLEMENTED();
	gfxFlushBuffers();
	gfxSwapBuffers();
	gspWaitForVBlank(); // FIXME: is this the right place to call, if we should at all?
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	C3D_FrameDrawOn(m_renderTarget);
	return S_OK;
}

void Citro3DRenderer::EnableTransparency()
{
	MINIWIN_NOT_IMPLEMENTED();
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
	MINIWIN_NOT_IMPLEMENTED();
}

HRESULT Citro3DRenderer::FinalizeFrame()
{
	MINIWIN_NOT_IMPLEMENTED();
	Mtx_PerspStereoTilt(&this->m_projectionMatrix, 40.0f * (acos(-1) / 180.0f), 400.0f / 240.0f, 0.01f, 1000.0f, 1, 2.0f, false);
			Mtx_Translate(&this->m_projectionMatrix, 0.0, 0.0, -10.0, 0);

			//Calculate model view matrix.
			C3D_Mtx modelView;
			Mtx_Identity(&modelView);
			// Mtx_Translate(&modelView, 0.0, 0.0, -2.0 + sinf(this->angleX));
			// Mtx_RotateX(&modelView, this->angleX, true);
			// Mtx_RotateY(&modelView, this->angleY, true);

			// if (interOcularDistance >= 0.0f){
			// 	this->angleX += radian;
			// 	this->angleY += radian;
			// }

			//Update uniforms
			C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, m_projectionShaderUniformLocation, &this->m_projectionMatrix);
			C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, modelViewUniformLocation, &modelView);

			//Draw the vertex buffer objects.
			C3D_DrawArrays(GPU_TRIANGLES, 0, sizeof(vertexList));
	C3D_FrameEnd(0);
	return S_OK;
}

void Citro3DRenderer::Resize(int width, int height, const ViewportTransform& viewportTransform)
{
	MINIWIN_NOT_IMPLEMENTED();
	m_width = width;
	m_height = height;
	m_viewportTransform = viewportTransform;

	SDL_DestroySurface(m_renderedImage);
	// FIXME: is this the right pixel format?
	m_renderedImage = SDL_CreateSurface(m_width, m_height, SDL_PIXELFORMAT_RGBA32);
}

void Citro3DRenderer::Clear(float r, float g, float b)
{
	u32 color =
		(static_cast<u32>(r * 255) << 24) | (static_cast<u32>(g * 255) << 16) | (static_cast<u32>(b * 255) << 8) | 255;
	C3D_RenderTargetClear(m_renderTarget, C3D_CLEAR_ALL, color, 0);
}

void Citro3DRenderer::Flip()
{
	MINIWIN_NOT_IMPLEMENTED();
}

void Citro3DRenderer::Draw2DImage(Uint32 textureId, const SDL_Rect& srcRect, const SDL_Rect& dstRect)
{
	MINIWIN_NOT_IMPLEMENTED();
	MINIWIN_TRACE("on draw 2d image");
	float left = -m_viewportTransform.offsetX / m_viewportTransform.scale;
	float right = (m_width - m_viewportTransform.offsetX) / m_viewportTransform.scale;
	float top = -m_viewportTransform.offsetY / m_viewportTransform.scale;
	float bottom = (m_height - m_viewportTransform.offsetY) / m_viewportTransform.scale;

	C3D_Mtx mtx;

	// TODO: isLeftHanded set to false. Should it be true?
	MINIWIN_TRACE("pre orthotilt");
	Mtx_OrthoTilt(&mtx, left, right, bottom, top, -1, 1, false);

	MINIWIN_TRACE("pre fvunifmtx4x4");
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, projectionShaderUniformLocation, &mtx);
}

void Citro3DRenderer::Download(SDL_Surface* target)
{
	MINIWIN_NOT_IMPLEMENTED();
}
