#include "d3drmrenderer_gxm.h"
#include "gxm_context.h"
#include "gxm_memory.h"
#include "meshutils.h"
#include "razor.h"
#include "shaders/gxm_shaders.h"
#include "tlsf.h"
#include "utils.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <psp2/common_dialog.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/types.h>
#include <string>

// from isleapp
extern bool g_dpadUp;
extern bool g_dpadDown;
extern bool g_dpadLeft;
extern bool g_dpadRight;

typedef struct GXMVertex {
	float position[3];
	float normal[3];
	float texCoord[2];
} GXMVertex;

Direct3DRMRenderer* GXMRenderer::Create(DWORD width, DWORD height, DWORD msaaSamples)
{
	int ret = gxm_library_init();
	if (ret < 0) {
		return nullptr;
	}

	SceGxmMultisampleMode msaaMode = SCE_GXM_MULTISAMPLE_NONE;
	if (msaaSamples == 2) {
		msaaMode = SCE_GXM_MULTISAMPLE_2X;
	}
	if (msaaSamples == 4) {
		msaaMode = SCE_GXM_MULTISAMPLE_4X;
	}

	if (!gxm) {
		gxm = (GXMContext*) SDL_malloc(sizeof(GXMContext));
		memset(gxm, 0, sizeof(GXMContext));
	}
	ret = SCE_ERR(gxm->init, msaaMode);
	if (ret < 0) {
		return nullptr;
	}

	return new GXMRenderer(width, height, msaaMode);
}

GXMRenderer::GXMRenderer(DWORD width, DWORD height, SceGxmMultisampleMode msaaMode)
{
	m_width = VITA_GXM_SCREEN_WIDTH;
	m_height = VITA_GXM_SCREEN_HEIGHT;
	m_virtualWidth = width;
	m_virtualHeight = height;

	// register shader programs
	int ret = SCE_ERR(
		sceGxmShaderPatcherRegisterProgram,
		gxm->shaderPatcher,
		mainVertexProgramGxp,
		&this->mainVertexProgramId
	);
	if (ret < 0) {
		return;
	}

	ret = SCE_ERR(
		sceGxmShaderPatcherRegisterProgram,
		gxm->shaderPatcher,
		mainColorFragmentProgramGxp,
		&this->mainColorFragmentProgramId
	);
	if (ret < 0) {
		return;
	}

	ret = SCE_ERR(
		sceGxmShaderPatcherRegisterProgram,
		gxm->shaderPatcher,
		mainTextureFragmentProgramGxp,
		&this->mainTextureFragmentProgramId
	);
	if (ret < 0) {
		return;
	}

	// main shader
	{
		GET_SHADER_PARAM(positionAttribute, mainVertexProgramGxp, "aPosition", );
		GET_SHADER_PARAM(normalAttribute, mainVertexProgramGxp, "aNormal", );
		GET_SHADER_PARAM(texCoordAttribute, mainVertexProgramGxp, "aTexCoord", );

		SceGxmVertexAttribute vertexAttributes[3];
		SceGxmVertexStream vertexStreams[1];

		// position
		vertexAttributes[0].streamIndex = 0;
		vertexAttributes[0].offset = 0;
		vertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
		vertexAttributes[0].componentCount = 3;
		vertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(positionAttribute);

		// normal
		vertexAttributes[1].streamIndex = 0;
		vertexAttributes[1].offset = 12;
		vertexAttributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
		vertexAttributes[1].componentCount = 3;
		vertexAttributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(normalAttribute);

		vertexAttributes[2].streamIndex = 0;
		vertexAttributes[2].offset = 24;
		vertexAttributes[2].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
		vertexAttributes[2].componentCount = 2;
		vertexAttributes[2].regIndex = sceGxmProgramParameterGetResourceIndex(texCoordAttribute);

		vertexStreams[0].stride = sizeof(GXMVertex);
		vertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

		ret = SCE_ERR(
			sceGxmShaderPatcherCreateVertexProgram,
			gxm->shaderPatcher,
			this->mainVertexProgramId,
			vertexAttributes,
			3,
			vertexStreams,
			1,
			&this->mainVertexProgram
		);
		if (ret < 0) {
			return;
		}
	}

	// main color opaque
	ret = SCE_ERR(
		sceGxmShaderPatcherCreateFragmentProgram,
		gxm->shaderPatcher,
		this->mainColorFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaaMode,
		&blendInfoOpaque,
		mainVertexProgramGxp,
		&this->opaqueColorFragmentProgram
	);
	if (ret < 0) {
		return;
	}

	// main color blended
	ret = SCE_ERR(
		sceGxmShaderPatcherCreateFragmentProgram,
		gxm->shaderPatcher,
		this->mainColorFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaaMode,
		&blendInfoTransparent,
		mainVertexProgramGxp,
		&this->blendedColorFragmentProgram
	);
	if (ret < 0) {
		return;
	}

	// main texture opaque
	ret = SCE_ERR(
		sceGxmShaderPatcherCreateFragmentProgram,
		gxm->shaderPatcher,
		this->mainTextureFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaaMode,
		&blendInfoOpaque,
		mainVertexProgramGxp,
		&this->opaqueTextureFragmentProgram
	);
	if (ret < 0) {
		return;
	}

	// main texture transparent
	ret = SCE_ERR(
		sceGxmShaderPatcherCreateFragmentProgram,
		gxm->shaderPatcher,
		this->mainTextureFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaaMode,
		&blendInfoTransparent,
		mainVertexProgramGxp,
		&this->blendedTextureFragmentProgram
	);
	if (ret < 0) {
		return;
	}

	// vertex uniforms
	this->uModelViewMatrix = sceGxmProgramFindParameterByName(mainVertexProgramGxp, "uModelViewMatrix");
	this->uNormalMatrix = sceGxmProgramFindParameterByName(mainVertexProgramGxp, "uNormalMatrix");
	this->uProjectionMatrix = sceGxmProgramFindParameterByName(mainVertexProgramGxp, "uProjectionMatrix");

	// fragment uniforms
	this->uLights = sceGxmProgramFindParameterByName(mainColorFragmentProgramGxp, "uLights"); // SceneLight[2]
	this->uAmbientLight = sceGxmProgramFindParameterByName(mainColorFragmentProgramGxp, "uAmbientLight"); // vec3
	this->uShininess = sceGxmProgramFindParameterByName(mainColorFragmentProgramGxp, "uShininess");       // float
	this->uColor = sceGxmProgramFindParameterByName(mainColorFragmentProgramGxp, "uColor");               // vec4

	for (int i = 0; i < GXM_FRAGMENT_BUFFER_COUNT; i++) {
		this->lights[i] = static_cast<GXMSceneLightUniform*>(gxm->alloc(sizeof(GXMSceneLightUniform), 4));
	}
	for (int i = 0; i < GXM_VERTEX_BUFFER_COUNT; i++) {
		this->quadVertices[i] = static_cast<GXMVertex2D*>(gxm->alloc(sizeof(GXMVertex2D) * 4 * 50, 4));
	}
	this->quadIndices = static_cast<uint16_t*>(gxm->alloc(sizeof(uint16_t) * 4, 4));
	this->quadIndices[0] = 0;
	this->quadIndices[1] = 1;
	this->quadIndices[2] = 2;
	this->quadIndices[3] = 3;

	volatile uint32_t* notificationMem = sceGxmGetNotificationRegion();
	for (uint32_t i = 0; i < GXM_FRAGMENT_BUFFER_COUNT; i++) {
		this->fragmentNotifications[i].address = notificationMem++;
		this->fragmentNotifications[i].value = 0;
	}
	this->currentFragmentBufferIndex = 0;

	for (uint32_t i = 0; i < GXM_VERTEX_BUFFER_COUNT; i++) {
		this->vertexNotifications[i].address = notificationMem++;
		this->vertexNotifications[i].value = 0;
	}
	this->currentVertexBufferIndex = 0;
	m_initialized = true;
}

GXMRenderer::~GXMRenderer()
{
	for (int i = 0; i < GXM_FRAGMENT_BUFFER_COUNT; i++) {
		if (this->lights[i]) {
			gxm->free(this->lights[i]);
		}
	}
	for (int i = 0; i < GXM_VERTEX_BUFFER_COUNT; i++) {
		if (this->quadVertices[i]) {
			gxm->free(this->quadVertices[i]);
		}
	}
	if (this->quadIndices) {
		gxm->free(this->quadIndices);
	}
}

void GXMRenderer::PushLights(const SceneLight* lightsArray, size_t count)
{
	if (count > 3) {
		SDL_Log("Unsupported number of lights (%d)", static_cast<int>(count));
		count = 3;
	}

	m_lights.assign(lightsArray, lightsArray + count);
}

void GXMRenderer::SetFrustumPlanes(const Plane* frustumPlanes)
{
}

void GXMRenderer::SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back)
{
	memcpy(&m_projection, projection, sizeof(D3DRMMATRIX4D));
}

struct TextureDestroyContextGXM {
	GXMRenderer* renderer;
	Uint32 textureId;
};

void GXMRenderer::AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture)
{
	auto* ctx = new TextureDestroyContextGXM{this, id};
	texture->AddDestroyCallback(
		[](IDirect3DRMObject* obj, void* arg) {
			auto* ctx = static_cast<TextureDestroyContextGXM*>(arg);
			auto& cache = ctx->renderer->m_textures[ctx->textureId];
			ctx->renderer->m_textures_delete[gxm->backBufferIndex].push_back(cache.gxmTexture);
			cache.texture = nullptr;
			memset(&cache.gxmTexture, 0, sizeof(SceGxmTexture));
			delete ctx;
		},
		ctx
	);
}

void GXMRenderer::DeferredDelete(int index)
{
	for (auto& del : this->m_textures_delete[index]) {
		void* textureData = sceGxmTextureGetData(&del);
		gxm->free(textureData);
	}
	this->m_textures_delete[index].clear();

	for (auto& del : this->m_buffers_delete[index]) {
		gxm->free(del);
	}
	this->m_buffers_delete[index].clear();
}

static int calculateMipLevels(int width, int height)
{
	if (width <= 0 || height <= 0) {
		return 1;
	}
	int maxDim = (width > height) ? width : height;
	return floor(log2(maxDim)) + 1;
}

static int nextPowerOf2(int n)
{
	if (n <= 0) {
		return 1;
	}
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n++;
	return n;
}

static void convertTextureMetadata(
	SDL_Surface* surface,
	bool isUi,
	bool* supportedFormat,
	SceGxmTextureFormat* gxmTextureFormat,
	size_t* textureSize,      // size in bytes
	size_t* textureAlignment, // alignment in bytes
	size_t* textureStride,    // stride in bytes
	size_t* paletteOffset,    // offset from textureData in bytes
	size_t* mipLevels
)
{
	int bytesPerPixel;
	size_t paletteSize = 0;
	*mipLevels = 1;

	switch (surface->format) {
	case SDL_PIXELFORMAT_INDEX8: {
		*supportedFormat = true;
		*gxmTextureFormat = SCE_GXM_TEXTURE_FORMAT_P8_ABGR;
		*textureAlignment = SCE_GXM_PALETTE_ALIGNMENT;
		bytesPerPixel = 1;
		if (!isUi) {
			*mipLevels = calculateMipLevels(surface->w, surface->h);
		}
		paletteSize = 256 * 4; // palette
		break;
	}
	case SDL_PIXELFORMAT_ABGR8888: {
		*supportedFormat = true;
		*gxmTextureFormat = SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR;
		*textureAlignment = SCE_GXM_TEXTURE_ALIGNMENT;
		bytesPerPixel = 4;
		if (!isUi) {
			*mipLevels = calculateMipLevels(surface->w, surface->h);
		}
		break;
	}
	default: {
		*supportedFormat = false;
		*gxmTextureFormat = SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR;
		*textureAlignment = SCE_GXM_TEXTURE_ALIGNMENT;
		bytesPerPixel = 4;
		break;
	}
	}
	*textureStride = ALIGN(surface->w, 8) * bytesPerPixel;

	*mipLevels = 1; // look weird

	size_t totalSize = 0;
	int currentW = surface->w;
	int currentH = surface->h;

	// top mip
	totalSize += (ALIGN(currentW, 8) * currentH) * bytesPerPixel;

	for (size_t i = 1; i < *mipLevels; ++i) {
		currentW = currentW > 1 ? currentW / 2 : 1;
		currentH = currentH > 1 ? currentH / 2 : 1;
		int po2W = nextPowerOf2(currentW * 2);
		int po2H = nextPowerOf2(currentH * 2);
		if (po2W < 8) {
			po2W = 8;
		}
		totalSize += po2W * po2H * bytesPerPixel;
	}

	if (paletteSize != 0) {
		int alignBytes = ALIGNMENT(totalSize, SCE_GXM_PALETTE_ALIGNMENT);
		totalSize += alignBytes;
		*paletteOffset = totalSize;
		totalSize += paletteSize;
	}

	*textureSize = totalSize;
}

void copySurfaceToGxmARGB888(SDL_Surface* src, uint8_t* textureData, size_t dstStride, size_t mipLevels)
{
	uint8_t* currentLevelData = textureData;
	uint32_t currentLevelWidth = src->w;
	uint32_t currentLevelHeight = src->h;
	size_t bytesPerPixel = 4;

	// copy top level mip (cant use transfer because this isnt gpu mapped)
	size_t topLevelStride = ALIGN(currentLevelWidth, 8) * bytesPerPixel;
	for (int y = 0; y < currentLevelHeight; y++) {
		uint8_t* srcRow = (uint8_t*) src->pixels + (y * src->pitch);
		uint8_t* dstRow = textureData + (y * topLevelStride);
		memcpy(dstRow, srcRow, currentLevelWidth * bytesPerPixel);
	}

	for (size_t i = 1; i < mipLevels; ++i) {
		uint32_t currentLevelSrcStride = SDL_max(currentLevelWidth, 8) * bytesPerPixel;
		uint32_t currentLevelDestStride = SDL_max((currentLevelWidth / 2), 8) * bytesPerPixel;
		uint8_t* nextLevelData = currentLevelData + (currentLevelSrcStride * currentLevelHeight);

		sceGxmTransferDownscale(
			SCE_GXM_TRANSFER_FORMAT_U8U8U8U8_ABGR, // src format
			currentLevelData,                      // src address
			0,
			0,
			currentLevelWidth,
			currentLevelHeight,                    // x,y,w,h
			currentLevelSrcStride,                 // stride
			SCE_GXM_TRANSFER_FORMAT_U8U8U8U8_ABGR, // dst format
			nextLevelData,                         // dst address
			0,
			0,                              // x,y
			currentLevelDestStride,         // stride
			NULL,                           // sync
			SCE_GXM_TRANSFER_FRAGMENT_SYNC, // flag
			NULL                            // notification
		);

		currentLevelData = nextLevelData;
		currentLevelWidth = currentLevelWidth / 2;
		currentLevelHeight = currentLevelHeight / 2;
	}
}

void copySurfaceToGxmIndexed8(
	DirectDrawSurfaceImpl* surface,
	SDL_Surface* src,
	uint8_t* textureData,
	size_t dstStride,
	uint8_t* paletteData,
	size_t mipLevels
)
{
	LPDIRECTDRAWPALETTE _palette;
	surface->GetPalette(&_palette);
	auto palette = static_cast<DirectDrawPaletteImpl*>(_palette);

	// copy palette
	memcpy(paletteData, palette->m_palette->colors, 256 * 4);

	uint8_t* currentLevelData = textureData;
	uint32_t currentLevelWidth = src->w;
	uint32_t currentLevelHeight = src->h;
	size_t bytesPerPixel = 1;

	// copy top level mip (cant use transfer because this isnt gpu mapped)
	size_t topLevelStride = ALIGN(currentLevelWidth, 8) * bytesPerPixel;
	for (int y = 0; y < currentLevelHeight; y++) {
		uint8_t* srcRow = (uint8_t*) src->pixels + (y * src->pitch);
		uint8_t* dstRow = textureData + (y * topLevelStride);
		memcpy(dstRow, srcRow, currentLevelWidth * bytesPerPixel);
	}

	for (size_t i = 1; i < mipLevels; ++i) {
		uint32_t currentLevelSrcStride = SDL_max(currentLevelWidth, 8) * bytesPerPixel;
		uint32_t currentLevelDestStride = SDL_max((currentLevelWidth / 2), 8) * bytesPerPixel;
		uint8_t* nextLevelData = currentLevelData + (currentLevelSrcStride * currentLevelHeight);

		sceGxmTransferDownscale(
			SCE_GXM_TRANSFER_FORMAT_U8_R, // src format
			currentLevelData,             // src address
			0,
			0,
			currentLevelWidth,
			currentLevelHeight,           // x,y,w,h
			currentLevelSrcStride,        // stride
			SCE_GXM_TRANSFER_FORMAT_U8_R, // dst format
			nextLevelData,                // dst address
			0,
			0,                              // x,y
			currentLevelDestStride,         // stride
			NULL,                           // sync
			SCE_GXM_TRANSFER_FRAGMENT_SYNC, // flag
			NULL                            // notification
		);

		currentLevelData = nextLevelData;
		currentLevelWidth = currentLevelWidth / 2;
		currentLevelHeight = currentLevelHeight / 2;
	}

	palette->Release();
}

void copySurfaceToGxm(
	DirectDrawSurfaceImpl* surface,
	uint8_t* textureData,
	size_t dstStride,
	size_t paletteOffset,
	size_t mipLevels
)
{
	SDL_Surface* src = surface->m_surface;

	switch (src->format) {
	case SDL_PIXELFORMAT_ABGR8888: {
		copySurfaceToGxmARGB888(src, textureData, dstStride, mipLevels);
		break;
	}
	case SDL_PIXELFORMAT_INDEX8: {
		copySurfaceToGxmIndexed8(surface, src, textureData, dstStride, textureData + paletteOffset, mipLevels);
		break;
	}
	default: {
		DEBUG_ONLY_PRINTF("unsupported format %d\n", SDL_GetPixelFormatName(src->format));
		SDL_Surface* dst = SDL_CreateSurfaceFrom(src->w, src->h, SDL_PIXELFORMAT_ABGR8888, textureData, src->w * 4);
		SDL_BlitSurface(src, nullptr, dst, nullptr);
		SDL_DestroySurface(dst);
		break;
	}
	}
}

Uint32 GXMRenderer::GetTextureId(IDirect3DRMTexture* iTexture, bool isUi, float scaleX, float scaleY)
{
	auto texture = static_cast<Direct3DRMTextureImpl*>(iTexture);
	auto surface = static_cast<DirectDrawSurfaceImpl*>(texture->m_surface);

	bool supportedFormat;
	SceGxmTextureFormat gxmTextureFormat;
	size_t textureSize;
	size_t textureAlignment;
	size_t textureStride;
	size_t paletteOffset;
	size_t mipLevels;

	int textureWidth = surface->m_surface->w;
	int textureHeight = surface->m_surface->h;

	convertTextureMetadata(
		surface->m_surface,
		isUi,
		&supportedFormat,
		&gxmTextureFormat,
		&textureSize,
		&textureAlignment,
		&textureStride,
		&paletteOffset,
		&mipLevels
	);

	if (!supportedFormat) {
		return NO_TEXTURE_ID;
	}

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (tex.texture == texture) {
			if (tex.version != texture->m_version) {
				sceGxmNotificationWait(tex.notification);
				tex.notification = &this->fragmentNotifications[this->currentFragmentBufferIndex];
				uint8_t* textureData = (uint8_t*) sceGxmTextureGetData(&tex.gxmTexture);
				copySurfaceToGxm(surface, textureData, textureStride, paletteOffset, mipLevels);
				tex.version = texture->m_version;
			}
			return i;
		}
	}

	DEBUG_ONLY_PRINTF(
		"Create Texture %s w=%d h=%d s=%d size=%d align=%d mips=%d\n",
		SDL_GetPixelFormatName(surface->m_surface->format),
		textureWidth,
		textureHeight,
		textureStride,
		textureSize,
		textureAlignment,
		mipLevels
	);

	// allocate gpu memory
	uint8_t* textureData = (uint8_t*) gxm->alloc(textureSize, textureAlignment);
	copySurfaceToGxm(surface, textureData, textureStride, paletteOffset, mipLevels);

	SceGxmTexture gxmTexture;
	SCE_ERR(
		sceGxmTextureInitLinear,
		&gxmTexture,
		textureData,
		gxmTextureFormat,
		textureWidth,
		textureHeight,
		mipLevels
	);
	if (isUi) {
		sceGxmTextureSetMinFilter(&gxmTexture, SCE_GXM_TEXTURE_FILTER_POINT);
		sceGxmTextureSetMagFilter(&gxmTexture, SCE_GXM_TEXTURE_FILTER_POINT);
		sceGxmTextureSetUAddrMode(&gxmTexture, SCE_GXM_TEXTURE_ADDR_CLAMP);
		sceGxmTextureSetVAddrMode(&gxmTexture, SCE_GXM_TEXTURE_ADDR_CLAMP);
	}
	else {
		sceGxmTextureSetMinFilter(&gxmTexture, SCE_GXM_TEXTURE_FILTER_LINEAR);
		sceGxmTextureSetMagFilter(&gxmTexture, SCE_GXM_TEXTURE_FILTER_LINEAR);
		sceGxmTextureSetUAddrMode(&gxmTexture, SCE_GXM_TEXTURE_ADDR_REPEAT);
		sceGxmTextureSetVAddrMode(&gxmTexture, SCE_GXM_TEXTURE_ADDR_REPEAT);
	}
	if (gxmTextureFormat == SCE_GXM_TEXTURE_FORMAT_P8_ABGR) {
		sceGxmTextureSetPalette(&gxmTexture, textureData + paletteOffset);
	}

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (!tex.texture) {
			memset(&tex, 0, sizeof(tex));
			tex.texture = texture;
			tex.version = texture->m_version;
			tex.gxmTexture = gxmTexture;
			tex.notification = &this->fragmentNotifications[this->currentFragmentBufferIndex];
			AddTextureDestroyCallback(i, texture);
			return i;
		}
	}

	GXMTextureCacheEntry tex;
	memset(&tex, 0, sizeof(tex));
	tex.texture = texture;
	tex.version = texture->m_version;
	tex.gxmTexture = gxmTexture;
	tex.notification = &this->fragmentNotifications[this->currentFragmentBufferIndex];
	m_textures.push_back(tex);
	Uint32 textureId = (Uint32) (m_textures.size() - 1);
	AddTextureDestroyCallback(textureId, texture);
	return textureId;
}

const SceGxmTexture* GXMRenderer::UseTexture(GXMTextureCacheEntry& texture)
{
	texture.notification = &this->fragmentNotifications[this->currentFragmentBufferIndex];
	sceGxmSetFragmentTexture(gxm->context, 0, &texture.gxmTexture);
	return &texture.gxmTexture;
}

GXMMeshCacheEntry GXMRenderer::GXMUploadMesh(const MeshGroup& meshGroup)
{
	GXMMeshCacheEntry cache{&meshGroup, meshGroup.version};

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
		std::transform(meshGroup.indices.begin(), meshGroup.indices.end(), indices.begin(), [](DWORD index) {
			return static_cast<uint16_t>(index);
		});
	}

	size_t vertexBufferSize = sizeof(GXMVertex) * vertices.size();
	size_t indexBufferSize = sizeof(uint16_t) * indices.size();
	void* meshData = gxm->alloc(vertexBufferSize + indexBufferSize, 4);

	GXMVertex* vertexBuffer = (GXMVertex*) meshData;
	uint16_t* indexBuffer = (uint16_t*) ((uint8_t*) meshData + vertexBufferSize);

	for (int i = 0; i < vertices.size(); i++) {
		D3DRMVERTEX vertex = vertices.data()[i];
		vertexBuffer[i] = GXMVertex{
			.position =
				{
					vertex.position.x,
					vertex.position.y,
					vertex.position.z,
				},
			.normal =
				{
					vertex.normal.x,
					vertex.normal.y,
					vertex.normal.z,
				},
			.texCoord =
				{
					vertex.tu,
					vertex.tv,
				}
		};
	}
	memcpy(indexBuffer, indices.data(), indices.size() * sizeof(uint16_t));

	cache.meshData = meshData;
	cache.vertexBuffer = vertexBuffer;
	cache.indexBuffer = indexBuffer;
	cache.indexCount = indices.size();
	return cache;
}

struct GXMMeshDestroyContext {
	GXMRenderer* renderer;
	Uint32 id;
};

void GXMRenderer::AddMeshDestroyCallback(Uint32 id, IDirect3DRMMesh* mesh)
{
	auto* ctx = new GXMMeshDestroyContext{this, id};
	mesh->AddDestroyCallback(
		[](IDirect3DRMObject*, void* arg) {
			auto* ctx = static_cast<GXMMeshDestroyContext*>(arg);
			auto& cache = ctx->renderer->m_meshes[ctx->id];
			cache.meshGroup = nullptr;
			ctx->renderer->m_buffers_delete[gxm->backBufferIndex].push_back(cache.meshData);
			cache.meshData = nullptr;
			cache.indexBuffer = nullptr;
			cache.vertexBuffer = nullptr;
			cache.indexCount = 0;
			delete ctx;
		},
		ctx
	);
}

Uint32 GXMRenderer::GetMeshId(IDirect3DRMMesh* mesh, const MeshGroup* meshGroup)
{
	for (Uint32 i = 0; i < m_meshes.size(); ++i) {
		auto& cache = m_meshes[i];
		if (cache.meshGroup == meshGroup) {
			if (cache.version != meshGroup->version) {
				cache = std::move(this->GXMUploadMesh(*meshGroup));
			}
			return i;
		}
	}

	auto newCache = this->GXMUploadMesh(*meshGroup);

	for (Uint32 i = 0; i < m_meshes.size(); ++i) {
		auto& cache = m_meshes[i];
		if (!cache.meshGroup) {
			cache = std::move(newCache);
			AddMeshDestroyCallback(i, mesh);
			return i;
		}
	}

	m_meshes.push_back(std::move(newCache));
	AddMeshDestroyCallback((Uint32) (m_meshes.size() - 1), mesh);
	return (Uint32) (m_meshes.size() - 1);
}

bool razor_live_started = false;
bool razor_display_enabled = false;

void GXMRenderer::StartScene()
{
	if (gxm->sceneStarted) {
		return;
	}

	this->DeferredDelete(gxm->frontBufferIndex);

#ifdef GXM_WITH_RAZOR
	bool dpad_up_clicked = !this->last_dpad_up && g_dpadUp;
	bool dpad_down_clicked = !this->last_dpad_down && g_dpadDown;
	bool dpad_left_clicked = !this->last_dpad_left && g_dpadLeft;
	bool dpad_right_clicked = !this->last_dpad_right && g_dpadRight;
	this->last_dpad_up = g_dpadUp;
	this->last_dpad_down = g_dpadDown;
	this->last_dpad_left = g_dpadLeft;
	this->last_dpad_right = g_dpadRight;

	if (with_razor_hud) {
		if (dpad_up_clicked) {
			razor_display_enabled = !razor_display_enabled;
			sceRazorHudSetDisplayEnabled(razor_display_enabled);
		}
		if (dpad_left_clicked) {
			if (razor_live_started) {
				sceRazorGpuLiveStop();
			}
			else {
				sceRazorGpuLiveStart();
			}
			razor_live_started = !razor_live_started;
		}
		if (dpad_right_clicked) {
			sceRazorGpuTraceTrigger();
		}
	}
	if (with_razor_capture) {
		if (dpad_down_clicked) {
			sceRazorGpuCaptureSetTriggerNextFrame("ux0:/data/capture.sgx");
		}
	}
#endif

	sceGxmBeginScene(
		gxm->context,
		SCE_GXM_SCENE_FRAGMENT_TRANSFER_SYNC,
		gxm->renderTarget,
		nullptr,
		nullptr,
		gxm->displayBuffersSync[gxm->backBufferIndex],
		&gxm->displayBuffersSurface[gxm->backBufferIndex],
		&gxm->depthSurface
	);
	sceGxmSetCullMode(gxm->context, SCE_GXM_CULL_CCW);
	gxm->sceneStarted = true;
	this->quadsUsed = 0;
	this->cleared = false;

	sceGxmNotificationWait(&this->vertexNotifications[this->currentVertexBufferIndex]);
	sceGxmNotificationWait(&this->fragmentNotifications[this->currentFragmentBufferIndex]);
}

HRESULT GXMRenderer::BeginFrame()
{
	this->transparencyEnabled = false;
	this->StartScene();

	auto lightData = this->LightsBuffer();
	int i = 0;
	for (const auto& light : m_lights) {
		if (!light.directional && !light.positional) {
			lightData->ambientLight[0] = light.color.r;
			lightData->ambientLight[1] = light.color.g;
			lightData->ambientLight[2] = light.color.b;
			continue;
		}
		if (i == 2) {
			sceClibPrintf("light overflow\n");
			continue;
		}

		lightData->lights[i].color[0] = light.color.r;
		lightData->lights[i].color[1] = light.color.g;
		lightData->lights[i].color[2] = light.color.b;
		lightData->lights[i].color[3] = light.color.a;

		bool isDirectional = light.directional == 1.0;
		if (isDirectional) {
			lightData->lights[i].vec[0] = light.direction.x;
			lightData->lights[i].vec[1] = light.direction.y;
			lightData->lights[i].vec[2] = light.direction.z;
		}
		else {
			lightData->lights[i].vec[0] = light.position.x;
			lightData->lights[i].vec[1] = light.position.y;
			lightData->lights[i].vec[2] = light.position.z;
		}
		lightData->lights[i].isDirectional = isDirectional;
		i++;
	}
	sceGxmSetFragmentUniformBuffer(gxm->context, 0, lightData);

	return DD_OK;
}

void GXMRenderer::EnableTransparency()
{
	this->transparencyEnabled = true;
}

void GXMRenderer::SubmitDraw(
	DWORD meshId,
	const D3DRMMATRIX4D& modelViewMatrix,
	const D3DRMMATRIX4D& worldMatrix,
	const D3DRMMATRIX4D& viewMatrix,
	const Matrix3x3& normalMatrix,
	const Appearance& appearance
)
{
	auto& mesh = m_meshes[meshId];

#ifdef DEBUG
	char marker[256];
	snprintf(marker, sizeof(marker), "SubmitDraw: %d", meshId);
	sceGxmPushUserMarker(gxm->context, marker);
#endif

	bool textured = appearance.textureId != NO_TEXTURE_ID;
	const SceGxmFragmentProgram* fragmentProgram;
	if (this->transparencyEnabled) {
		fragmentProgram = textured ? this->blendedTextureFragmentProgram : this->blendedColorFragmentProgram;
	}
	else {
		fragmentProgram = textured ? this->opaqueTextureFragmentProgram : this->opaqueColorFragmentProgram;
	}
	sceGxmSetVertexProgram(gxm->context, this->mainVertexProgram);
	sceGxmSetFragmentProgram(gxm->context, fragmentProgram);

	void* vertUniforms;
	void* fragUniforms;
	sceGxmReserveVertexDefaultUniformBuffer(gxm->context, &vertUniforms);
	sceGxmReserveFragmentDefaultUniformBuffer(gxm->context, &fragUniforms);

	// vertex uniforms
	sceGxmSetUniformDataF(vertUniforms, this->uModelViewMatrix, 0, 4 * 4, &modelViewMatrix[0][0]);
	sceGxmSetUniformDataF(vertUniforms, this->uNormalMatrix, 0, 3 * 3, &normalMatrix[0][0]);
	sceGxmSetUniformDataF(vertUniforms, this->uProjectionMatrix, 0, 4 * 4, &this->m_projection[0][0]);

	// fragment uniforms
	float color[4] = {
		appearance.color.r / 255.0f,
		appearance.color.g / 255.0f,
		appearance.color.b / 255.0f,
		appearance.color.a / 255.0f
	};
	sceGxmSetUniformDataF(fragUniforms, this->uColor, 0, 4, color);
	sceGxmSetUniformDataF(fragUniforms, this->uShininess, 0, 1, &appearance.shininess);

	if (textured) {
		auto& texture = m_textures[appearance.textureId];
		this->UseTexture(texture);
	}
	sceGxmSetVertexStream(gxm->context, 0, mesh.vertexBuffer);
	sceGxmSetFrontDepthFunc(gxm->context, SCE_GXM_DEPTH_FUNC_LESS_EQUAL);
	sceGxmDraw(gxm->context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, mesh.indexBuffer, mesh.indexCount);

#ifdef DEBUG
	sceGxmPopUserMarker(gxm->context);
#endif
}

HRESULT GXMRenderer::FinalizeFrame()
{
	return DD_OK;
}

void GXMRenderer::Resize(int width, int height, const ViewportTransform& viewportTransform)
{
	m_width = width;
	m_height = height;
	m_viewportTransform = viewportTransform;
}

void GXMRenderer::Clear(float r, float g, float b)
{
	this->StartScene();
	gxm->clear(r, g, b, false);
	this->cleared = true;
}

void GXMRenderer::Flip()
{
	if (!gxm->sceneStarted) {
		return;
	}

	++this->vertexNotifications[this->currentVertexBufferIndex].value;
	++this->fragmentNotifications[this->currentFragmentBufferIndex].value;
	sceGxmEndScene(
		gxm->context,
		&this->vertexNotifications[this->currentVertexBufferIndex],
		&this->fragmentNotifications[this->currentFragmentBufferIndex]
	);
	gxm->sceneStarted = false;

	this->currentVertexBufferIndex = (this->currentVertexBufferIndex + 1) % GXM_VERTEX_BUFFER_COUNT;
	this->currentFragmentBufferIndex = (this->currentFragmentBufferIndex + 1) % GXM_FRAGMENT_BUFFER_COUNT;
	gxm->swap_display();
}

void GXMRenderer::Draw2DImage(Uint32 textureId, const SDL_Rect& srcRect, const SDL_Rect& dstRect, FColor color)
{
	this->StartScene();
	if (!this->cleared) {
		gxm->clear(0, 0, 0, false);
		this->cleared = true;
	}

#ifdef DEBUG
	char marker[256];
	snprintf(marker, sizeof(marker), "Draw2DImage: %d", textureId);
	sceGxmPushUserMarker(gxm->context, marker);
#endif

	sceGxmSetVertexProgram(gxm->context, gxm->planeVertexProgram);
	if (textureId != NO_TEXTURE_ID) {
		sceGxmSetFragmentProgram(gxm->context, gxm->imageFragmentProgram);
	}
	else {
		sceGxmSetFragmentProgram(gxm->context, gxm->colorFragmentProgram);
	}

	void* vertUniforms;
	void* fragUniforms;
	sceGxmReserveVertexDefaultUniformBuffer(gxm->context, &vertUniforms);
	sceGxmReserveFragmentDefaultUniformBuffer(gxm->context, &fragUniforms);

	float left = -this->m_viewportTransform.offsetX / this->m_viewportTransform.scale;
	float right = (this->m_width - this->m_viewportTransform.offsetX) / this->m_viewportTransform.scale;
	float top = -this->m_viewportTransform.offsetY / this->m_viewportTransform.scale;
	float bottom = (this->m_height - this->m_viewportTransform.offsetY) / this->m_viewportTransform.scale;

#define virtualToNDCX(x) (((x - left) / (right - left)) * 2 - 1);
#define virtualToNDCY(y) -(((y - top) / (bottom - top)) * 2 - 1);

	float x1_virtual = static_cast<float>(dstRect.x);
	float y1_virtual = static_cast<float>(dstRect.y);
	float x2_virtual = x1_virtual + dstRect.w;
	float y2_virtual = y1_virtual + dstRect.h;

	float x1 = virtualToNDCX(x1_virtual);
	float y1 = virtualToNDCY(y1_virtual);
	float x2 = virtualToNDCX(x2_virtual);
	float y2 = virtualToNDCY(y2_virtual);

	float u1 = 0.0;
	float v1 = 0.0;
	float u2 = 0.0;
	float v2 = 0.0;

	if (textureId != NO_TEXTURE_ID) {
		GXMTextureCacheEntry& texture = m_textures[textureId];
		const SceGxmTexture* gxmTexture = this->UseTexture(texture);
		float texW = sceGxmTextureGetWidth(gxmTexture);
		float texH = sceGxmTextureGetHeight(gxmTexture);

		u1 = static_cast<float>(srcRect.x) / texW;
		v1 = static_cast<float>(srcRect.y) / texH;
		u2 = static_cast<float>(srcRect.x + srcRect.w) / texW;
		v2 = static_cast<float>(srcRect.y + srcRect.h) / texH;
	}
	else {
		SET_UNIFORM(fragUniforms, gxm->color_uColor, color);
	}

	GXMVertex2D* quadVertices = this->QuadVerticesBuffer();
	quadVertices[0] = GXMVertex2D{.position = {x1, y1}, .texCoord = {u1, v1}};
	quadVertices[1] = GXMVertex2D{.position = {x2, y1}, .texCoord = {u2, v1}};
	quadVertices[2] = GXMVertex2D{.position = {x1, y2}, .texCoord = {u1, v2}};
	quadVertices[3] = GXMVertex2D{.position = {x2, y2}, .texCoord = {u2, v2}};

	sceGxmSetVertexStream(gxm->context, 0, quadVertices);

	sceGxmSetFrontDepthWriteEnable(gxm->context, SCE_GXM_DEPTH_WRITE_DISABLED);
	sceGxmSetFrontDepthFunc(gxm->context, SCE_GXM_DEPTH_FUNC_ALWAYS);
	sceGxmDraw(gxm->context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, this->quadIndices, 4);
	sceGxmSetFrontDepthWriteEnable(gxm->context, SCE_GXM_DEPTH_WRITE_ENABLED);

#ifdef DEBUG
	sceGxmPopUserMarker(gxm->context);
#endif
}

void GXMRenderer::SetDither(bool dither)
{
}

void GXMRenderer::Download(SDL_Surface* target)
{
	SDL_Rect srcRect = {
		static_cast<int>(m_viewportTransform.offsetX),
		static_cast<int>(m_viewportTransform.offsetY),
		static_cast<int>(target->w * m_viewportTransform.scale),
		static_cast<int>(target->h * m_viewportTransform.scale),
	};
	SDL_Surface* src = SDL_CreateSurfaceFrom(
		VITA_GXM_SCREEN_WIDTH,
		VITA_GXM_SCREEN_HEIGHT,
		SDL_PIXELFORMAT_ABGR8888,
		gxm->displayBuffers[gxm->frontBufferIndex],
		VITA_GXM_SCREEN_STRIDE * 4
	);
	SDL_BlitSurfaceScaled(src, &srcRect, target, nullptr, SDL_SCALEMODE_NEAREST);
	SDL_DestroySurface(src);
}
