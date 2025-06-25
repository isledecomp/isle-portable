#include "ddpalette_impl.h"
#include "ddraw_impl.h"
#include "dummysurface_impl.h"
#include "framebuffer_impl.h"
#include "miniwin.h"

#include <assert.h>

#include "../d3drm/backends/gxm/memory.h"
#include "../d3drm/backends/gxm/utils.h"

FrameBufferImpl::FrameBufferImpl(LPDDSURFACEDESC lpDDSurfaceDesc)
{
	this->width = 960;
	this->height = 544;

	if(!get_gxm_context(&this->context, &this->shaderPatcher, &this->cdramPool)) {
		return;
	}

	// render target
	SceGxmRenderTargetParams renderTargetParams;
    memset(&renderTargetParams, 0, sizeof(SceGxmRenderTargetParams));
    renderTargetParams.flags = 0;
    renderTargetParams.width = this->width;
    renderTargetParams.height = this->height;
    renderTargetParams.scenesPerFrame = 1;
    renderTargetParams.multisampleMode = 0;
    renderTargetParams.multisampleLocations = 0;
    renderTargetParams.driverMemBlock = -1; // Invalid UID

	if(SCE_ERR(sceGxmCreateRenderTarget, &renderTargetParams, &this->renderTarget)) {
		return;
	}

	for(int i = 0; i < VITA_GXM_DISPLAY_BUFFER_COUNT; i++) {
		this->displayBuffers[i].data = vita_mem_alloc(
			SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
			4 * this->width * this->height,
			SCE_GXM_COLOR_SURFACE_ALIGNMENT,
			SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
			&this->displayBuffers[i].uid, "display", nullptr);

		if(SCE_ERR(sceGxmColorSurfaceInit,
			&this->displayBuffers[i].surface,
			SCE_GXM_COLOR_FORMAT_A8B8G8R8,
			SCE_GXM_COLOR_SURFACE_LINEAR,
			SCE_GXM_COLOR_SURFACE_SCALE_NONE,
			SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
			width, height, width,
			this->displayBuffers[i].data
		)) {
			return;
		};

		if(SCE_ERR(sceGxmSyncObjectCreate, &this->displayBuffers[i].sync)) {
			return;
		}
	}


	if(SCE_ERR(sceGxmShaderPatcherRegisterProgram, this->shaderPatcher, blitVertexProgramGxp, &this->blitVertexProgramId)) {
		return;
	}
	if(SCE_ERR(sceGxmShaderPatcherRegisterProgram, this->shaderPatcher, blitColorFragmentProgramGxp, &this->blitColorFragmentProgramId)) {
		return;
	}
	if(SCE_ERR(sceGxmShaderPatcherRegisterProgram, this->shaderPatcher, blitTexFragmentProgramGxp, &this->blitTexFragmentProgramId)) {
		return;
	}
	GET_SHADER_PARAM(positionAttribute, blitVertexProgramGxp, "aPosition",);
	GET_SHADER_PARAM(texCoordAttribute, blitVertexProgramGxp, "aTexCoord",);

	SceGxmVertexAttribute vertexAttributes[1];
	SceGxmVertexStream vertexStreams[1];
	vertexAttributes[0].streamIndex = 0;
	vertexAttributes[0].offset = 0;
	vertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	vertexAttributes[0].componentCount = 2;
	vertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(positionAttribute);
	vertexAttributes[1].streamIndex = 0;
	vertexAttributes[1].offset = 8;
	vertexAttributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	vertexAttributes[1].componentCount = 2;
	vertexAttributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(texCoordAttribute);
	vertexStreams[0].stride = sizeof(float)*4;
	vertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

	if(SCE_ERR(sceGxmShaderPatcherCreateVertexProgram,
		this->shaderPatcher,
		this->blitVertexProgramId,
		vertexAttributes, 2,
		vertexStreams, 1,
		&this->blitVertexProgram
	)) {
		return;
	}

	if(SCE_ERR(sceGxmShaderPatcherCreateFragmentProgram,
		this->shaderPatcher,
		this->blitColorFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		SCE_GXM_MULTISAMPLE_NONE,
		NULL,
		blitVertexProgramGxp,
		&this->blitColorFragmentProgram
	)) {
		return;
	}

	if(SCE_ERR(sceGxmShaderPatcherCreateFragmentProgram,
		this->shaderPatcher,
		this->blitTexFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		SCE_GXM_MULTISAMPLE_NONE,
		NULL,
		blitVertexProgramGxp,
		&this->blitTexFragmentProgram
	)) {
		return;
	}

	this->uScreenMatrix = sceGxmProgramFindParameterByName(blitVertexProgramGxp, "uScreenMatrix"); // matrix4

	this->uColor = sceGxmProgramFindParameterByName(blitColorFragmentProgramGxp, "uColor"); // vec4
	this->uTexMatrix = sceGxmProgramFindParameterByName(blitTexFragmentProgramGxp, "uTexMatrix"); // matrix4

	const size_t quadMeshVerticiesSize = 4 * sizeof(float)*4;
	const size_t quadMeshIndiciesSize = 4 * sizeof(uint16_t);
	this->quadMeshBuffer = sceClibMspaceMalloc(this->cdramPool, quadMeshVerticiesSize + quadMeshIndiciesSize);
	this->quadVerticies = (float*)this->quadMeshBuffer;
	this->quadIndicies = (uint16_t*)((uint8_t*)(this->quadMeshBuffer)+quadMeshVerticiesSize);

	float quadVerts[] = {
		// x,     y,     u,    v
		-1.0f, -1.0f,  0.0f, 1.0f,  // Bottom-left
		-1.0f,  1.0f,  0.0f, 0.0f,  // Top-left
		1.0f, -1.0f,  1.0f, 1.0f,  // Bottom-right
		1.0f,  1.0f,  1.0f, 0.0f   // Top-right
	};
	memcpy(this->quadVerticies, quadVerts, quadMeshVerticiesSize);
	this->quadIndicies[0] = 0;
	this->quadIndicies[1] = 1;
	this->quadIndicies[2] = 2;
	this->quadIndicies[3] = 3;

	this->backBufferIndex = 0;
	this->frontBufferIndex = 1;

	DDBackBuffer = SDL_CreateSurfaceFrom(this->width, this->height, SDL_PIXELFORMAT_RGBA8888, nullptr, 0);
	if (!DDBackBuffer) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create surface: %s", SDL_GetError());
	}

	DDBackBuffer->pitch = this->width*4;
	DDBackBuffer->pixels = this->backBuffer()->data;
}

FrameBufferImpl::~FrameBufferImpl()
{
	if (m_palette) {
		m_palette->Release();
	}
}

// IUnknown interface
HRESULT FrameBufferImpl::QueryInterface(const GUID& riid, void** ppvObject)
{
	MINIWIN_NOT_IMPLEMENTED();
	return E_NOINTERFACE;
}

// IDirectDrawSurface interface
HRESULT FrameBufferImpl::AddAttachedSurface(LPDIRECTDRAWSURFACE lpDDSAttachedSurface)
{
	if (dynamic_cast<DummySurfaceImpl*>(lpDDSAttachedSurface)) {
		return DD_OK;
	}
	MINIWIN_NOT_IMPLEMENTED();
	return DDERR_GENERIC;
}

#include <cstdio>

void calculateTexMatrix(SDL_Rect srcRect, int textureWidth, int textureHeight, float matrix[4][4]) {
    float scaleX = srcRect.w / (float)textureWidth;
    float scaleY = srcRect.h / (float)textureHeight;
    float offsetX = srcRect.x / (float)textureWidth;
    float offsetY = srcRect.y / (float)textureHeight;
    memset(matrix, 0, sizeof(float) * 16);
    matrix[0][0] = scaleX;
    matrix[1][1] = scaleY;
    matrix[2][2] = 1.0f;
    matrix[3][3] = 1.0f;
    matrix[0][3] = offsetX;
    matrix[1][3] = offsetY;
}

void calculateScreenMatrix(SDL_Rect dstRect, int screenWidth, int screenHeight, float matrix[4][4]) {
    float scaleX = (2.0f * dstRect.w) / screenWidth;
    float scaleY = (2.0f * dstRect.h) / screenHeight;
    float offsetX = (2.0f * dstRect.x) / screenWidth - 1.0f + scaleX * 0.5f;
    float offsetY = 1.0f - (2.0f * dstRect.y) / screenHeight - scaleY * 0.5f;
    memset(matrix, 0, sizeof(float) * 16);
    matrix[0][0] = scaleX;
    matrix[1][1] = scaleY;
    matrix[2][2] = 1.0f;
    matrix[3][3] = 1.0f;
    matrix[0][3] = offsetX;
    matrix[1][3] = offsetY;
}
HRESULT FrameBufferImpl::Blt(
	LPRECT lpDestRect,
	LPDIRECTDRAWSURFACE lpDDSrcSurface,
	LPRECT lpSrcRect,
	DDBltFlags dwFlags,
	LPDDBLTFX lpDDBltFx
) 
{
	if (dynamic_cast<FrameBufferImpl*>(lpDDSrcSurface) == this) {
		return Flip(nullptr, DDFLIP_WAIT);
	}

	if(!sceneStarted) {
		SCE_ERR(sceGxmBeginScene,
			this->context,
			0,
			this->renderTarget,
			nullptr,
			nullptr,
			nullptr,
			&this->backBuffer()->surface,
			nullptr
		);

		sceGxmSetFrontStencilFunc(
			this->context,
			SCE_GXM_STENCIL_FUNC_NEVER,
			SCE_GXM_STENCIL_OP_KEEP,
			SCE_GXM_STENCIL_OP_KEEP,
			SCE_GXM_STENCIL_OP_KEEP,
			0xFF,
			0xFF
		);
		sceGxmSetFrontDepthFunc(this->context, SCE_GXM_DEPTH_FUNC_ALWAYS);

		sceneStarted = true;
	}

	bool colorFill = ((dwFlags & DDBLT_COLORFILL) == DDBLT_COLORFILL);
	sceGxmSetVertexProgram(this->context, this->blitVertexProgram);
	SCE_ERR(sceGxmSetVertexStream, this->context, 0, this->quadVerticies);

	if(colorFill) {
		sceGxmSetFragmentProgram(this->context, this->blitColorFragmentProgram);
	} else {
		sceGxmSetFragmentProgram(this->context, this->blitTexFragmentProgram);
	}

	void* vertUniforms;
	void* fragUniforms;
	SCE_ERR(sceGxmReserveVertexDefaultUniformBuffer, this->context, &vertUniforms);
	SCE_ERR(sceGxmReserveFragmentDefaultUniformBuffer, this->context, &fragUniforms);

	if(colorFill) {
		SDL_Rect rect = {0, 0, this->width, this->height};

		DirectDrawPaletteImpl* ddPal = static_cast<DirectDrawPaletteImpl*>(m_palette);
		SDL_Palette* sdlPalette = ddPal ? ddPal->m_palette : nullptr;
		uint8_t r,g,b,a;
		SDL_GetRGBA(lpDDBltFx->dwFillColor, SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_ABGR8888), sdlPalette, &r, &g, &b, &a);

		float screenMatrix[4][4];
		calculateScreenMatrix(rect, this->width, this->height, screenMatrix);
		SET_UNIFORM(vertUniforms, this->uScreenMatrix, screenMatrix);

		float color[4] = {
			(float)r / 255.0f,
			(float)g / 255.0f,
			(float)b / 255.0f,
			(float)a / 255.0f
		};
		SET_UNIFORM(fragUniforms, this->uColor, color);
	} else {
		auto other = static_cast<DirectDrawSurfaceImpl*>(lpDDSrcSurface);
		if (!other) {
			return DDERR_GENERIC;
		}
		SDL_Surface* blitSource = other->m_surface;

		SDL_Rect srcRect = lpSrcRect ? ConvertRect(lpSrcRect) : SDL_Rect{0, 0, other->m_surface->w, other->m_surface->h};
		SDL_Rect dstRect = lpDestRect ? ConvertRect(lpDestRect) : SDL_Rect{0, 0, this->width, this->height};

		float screenMatrix[4][4];
		calculateScreenMatrix(dstRect, this->width, this->height, screenMatrix);
		SET_UNIFORM(vertUniforms, this->uScreenMatrix, screenMatrix);

		float texMatrix[4][4];
		calculateTexMatrix(srcRect, blitSource->w, blitSource->h, texMatrix);
		SET_UNIFORM(vertUniforms, this->uTexMatrix, texMatrix);

		SceGxmTextureFormat texFormat = SCE_GXM_TEXTURE_FORMAT_A8B8G8R8;
		SceGxmTexture gxmTexture;
		SCE_ERR(sceGxmTextureInitLinear, &gxmTexture, blitSource->pixels, texFormat, blitSource->w, blitSource->h, 0);
		sceGxmTextureSetStride(&gxmTexture, blitSource->pitch);
		SCE_ERR(sceGxmTextureSetMinFilter, &gxmTexture, SCE_GXM_TEXTURE_FILTER_POINT);
		SCE_ERR(sceGxmTextureSetMagFilter, &gxmTexture, SCE_GXM_TEXTURE_FILTER_POINT);
		SCE_ERR(sceGxmSetFragmentTexture, this->context, 0, &gxmTexture);
	}
	
	SCE_ERR(sceGxmDraw,
		this->context,
		SCE_GXM_PRIMITIVE_TRIANGLE_STRIP,
		SCE_GXM_INDEX_FORMAT_U16,
		this->quadIndicies,
		4
	);
	return DD_OK;
}

HRESULT FrameBufferImpl::BltFast(
	DWORD dwX,
	DWORD dwY,
	LPDIRECTDRAWSURFACE lpDDSrcSurface,
	LPRECT lpSrcRect,
	DDBltFastFlags dwTrans
)
{
	RECT destRect = {
		(int) dwX,
		(int) dwY,
		(int) (lpSrcRect->right - lpSrcRect->left + dwX),
		(int) (lpSrcRect->bottom - lpSrcRect->top + dwY)
	};
	return Blt(&destRect, lpDDSrcSurface, lpSrcRect, DDBLT_NONE, nullptr);
}

HRESULT FrameBufferImpl::Flip(LPDIRECTDRAWSURFACE lpDDSurfaceTargetOverride, DDFlipFlags dwFlags)
{
	if(this->sceneStarted) {
		sceGxmEndScene(this->context, nullptr, nullptr);
		this->sceneStarted = false;
	}

	sceGxmPadHeartbeat(
		&this->displayBuffers[this->backBufferIndex].surface,
		this->displayBuffers[this->backBufferIndex].sync
	);

	VITA_GXM_DisplayData displayData;
	displayData.address = this->displayBuffers[this->backBufferIndex].data;

	sceGxmDisplayQueueAddEntry(
		this->displayBuffers[this->frontBufferIndex].sync,
		this->displayBuffers[this->backBufferIndex].sync,
		&displayData
	);

	this->frontBufferIndex = this->backBufferIndex;
    this->backBufferIndex = (this->backBufferIndex + 1) % VITA_GXM_DISPLAY_BUFFER_COUNT;

	DDBackBuffer->pixels = this->backBuffer()->data;
	return DD_OK;
}

HRESULT FrameBufferImpl::GetAttachedSurface(LPDDSCAPS lpDDSCaps, LPDIRECTDRAWSURFACE* lplpDDAttachedSurface)
{
	if ((lpDDSCaps->dwCaps & DDSCAPS_BACKBUFFER) != DDSCAPS_BACKBUFFER) {
		return DDERR_INVALIDPARAMS;
	}
	*lplpDDAttachedSurface = static_cast<IDirectDrawSurface*>(this);
	return DD_OK;
}

HRESULT FrameBufferImpl::GetDC(HDC* lphDC)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT FrameBufferImpl::GetPalette(LPDIRECTDRAWPALETTE* lplpDDPalette)
{
	if (!m_palette) {
		return DDERR_GENERIC;
	}
	m_palette->AddRef();
	*lplpDDPalette = m_palette;
	return DD_OK;
}

HRESULT FrameBufferImpl::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat)
{
	memset(lpDDPixelFormat, 0, sizeof(*lpDDPixelFormat));
	lpDDPixelFormat->dwFlags = DDPF_RGB;
	lpDDPixelFormat->dwFlags |= DDPF_PALETTEINDEXED8;
	lpDDPixelFormat->dwRGBBitCount = 8;
	lpDDPixelFormat->dwRBitMask = 0xff;
	lpDDPixelFormat->dwGBitMask = 0xff;
	lpDDPixelFormat->dwBBitMask = 0xff;
	lpDDPixelFormat->dwRGBAlphaBitMask = 0xff;
	return DD_OK;
}

HRESULT FrameBufferImpl::GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc)
{
	lpDDSurfaceDesc->dwFlags = DDSD_PIXELFORMAT;
	GetPixelFormat(&lpDDSurfaceDesc->ddpfPixelFormat);
	lpDDSurfaceDesc->dwFlags |= DDSD_WIDTH | DDSD_HEIGHT;
	lpDDSurfaceDesc->dwWidth = this->width;
	lpDDSurfaceDesc->dwHeight = this->height;
	return DD_OK;
}

HRESULT FrameBufferImpl::IsLost()
{
	return DD_OK;
}

HRESULT FrameBufferImpl::Lock(LPRECT lpDestRect, DDSURFACEDESC* lpDDSurfaceDesc, DDLockFlags dwFlags, HANDLE hEvent)
{
	GetSurfaceDesc(lpDDSurfaceDesc);
	lpDDSurfaceDesc->lpSurface = this->backBuffer()->data;
	lpDDSurfaceDesc->lPitch = this->width*4;

	return DD_OK;
}

HRESULT FrameBufferImpl::ReleaseDC(HDC hDC)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT FrameBufferImpl::Restore()
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT FrameBufferImpl::SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper)
{
	return DD_OK;
}

HRESULT FrameBufferImpl::SetColorKey(DDColorKeyFlags dwFlags, LPDDCOLORKEY lpDDColorKey)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT FrameBufferImpl::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette)
{
	if (m_palette) {
		m_palette->Release();
	}

	m_palette = lpDDPalette;
	//SDL_SetSurfacePalette(DDBackBuffer, ((DirectDrawPaletteImpl*) m_palette)->m_palette);
	m_palette->AddRef();
	return DD_OK;
}

HRESULT FrameBufferImpl::Unlock(LPVOID lpSurfaceData)
{
	return DD_OK;
}
