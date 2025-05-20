#include "miniwin_d3drm.h"

#include "miniwin_ddsurface_p.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <assert.h>
#include <vector>

typedef struct PositionColorVertex {
	float x, y, z;
	Uint8 r, g, b, a;
} PositionColorVertex;

template <typename InterfaceType, typename ArrayInterface>
class Direct3DRMArrayBase : public ArrayInterface {
public:
	~Direct3DRMArrayBase() override
	{
		for (auto* item : items) {
			if (item) {
				item->Release();
			}
		}
	}
	DWORD GetSize() override { return static_cast<DWORD>(items.size()); }
	HRESULT AddElement(InterfaceType* in) override
	{
		if (!in) {
			return DDERR_INVALIDPARAMS;
		}
		in->AddRef();
		items.push_back(in);
		return DD_OK;
	}
	HRESULT GetElement(DWORD index, InterfaceType** out) override
	{
		if (index >= items.size()) {
			return DDERR_INVALIDPARAMS;
		}
		*out = static_cast<InterfaceType*>(items[index]);
		if (*out) {
			(*out)->AddRef();
		}
		return DD_OK;
	}
	HRESULT DeleteElement(InterfaceType* element) override
	{
		auto it = std::find(items.begin(), items.end(), element);
		if (it == items.end()) {
			return DDERR_INVALIDPARAMS;
		}

		(*it)->Release();
		items.erase(it);
		return DD_OK;
	}

protected:
	std::vector<InterfaceType*> items;
};

struct Direct3DRMFrameArrayImpl : public Direct3DRMArrayBase<IDirect3DRMFrame, IDirect3DRMFrameArray> {
	using Direct3DRMArrayBase::Direct3DRMArrayBase;
};

struct Direct3DRMLightArrayImpl : public Direct3DRMArrayBase<IDirect3DRMLight, IDirect3DRMLightArray> {
	using Direct3DRMArrayBase::Direct3DRMArrayBase;
};

struct Direct3DRMViewportArrayImpl : public Direct3DRMArrayBase<IDirect3DRMViewport, IDirect3DRMViewportArray> {
	using Direct3DRMArrayBase::Direct3DRMArrayBase;
};

struct Direct3DRMVisualArrayImpl : public Direct3DRMArrayBase<IDirect3DRMVisual, IDirect3DRMVisualArray> {
	using Direct3DRMArrayBase::Direct3DRMArrayBase;
};

struct PickRecord {
	IDirect3DRMVisual* visual;
	IDirect3DRMFrameArray* frameArray;
	D3DRMPICKDESC desc;
};

struct Direct3DRMPickedArrayImpl : public IDirect3DRMPickedArray {
	~Direct3DRMPickedArrayImpl() override
	{
		for (PickRecord& pick : picks) {
			if (pick.visual) {
				pick.visual->Release();
			}
			if (pick.frameArray) {
				pick.frameArray->Release();
			}
		}
	}
	DWORD GetSize() override { return static_cast<DWORD>(picks.size()); }
	HRESULT GetPick(DWORD index, IDirect3DRMVisual** visual, IDirect3DRMFrameArray** frameArray, D3DRMPICKDESC* desc)
		override
	{
		if (index >= picks.size()) {
			return DDERR_INVALIDPARAMS;
		}

		const PickRecord& pick = picks[index];

		*visual = pick.visual;
		*frameArray = pick.frameArray;
		*desc = pick.desc;

		if (*visual) {
			(*visual)->AddRef();
		}
		if (*frameArray) {
			(*frameArray)->AddRef();
		}

		return DD_OK;
	}

private:
	std::vector<PickRecord> picks;
};

struct Direct3DRMWinDeviceImpl : public IDirect3DRMWinDevice {
	HRESULT Activate() override { return DD_OK; }
	HRESULT Paint() override { return DD_OK; }
	void HandleActivate(WORD wParam) override {}
	void HandlePaint(void* p_dc) override {}
};

template <typename T>
struct Direct3DRMObjectBase : public T {
	ULONG Release() override
	{
		if (IUnknown::m_refCount == 1) {
			for (auto it = m_callbacks.cbegin(); it != m_callbacks.cend(); it++) {
				it->first(this, it->second);
			}
		}
		return this->T::Release();
	}
	HRESULT AddDestroyCallback(D3DRMOBJECTCALLBACK callback, void* arg) override
	{
		m_callbacks.push_back(std::make_pair(callback, arg));
		return D3DRM_OK;
	}
	HRESULT DeleteDestroyCallback(D3DRMOBJECTCALLBACK callback, void* arg) override
	{
		for (auto it = m_callbacks.cbegin(); it != m_callbacks.cend(); it++) {
			if (it->first == callback && it->second == arg) {
				m_callbacks.erase(it);
				return D3DRM_OK;
			}
		}
		return D3DRMERR_NOTFOUND;
	}
	HRESULT SetAppData(LPD3DRM_APPDATA appData) override
	{
		m_appData = appData;
		return D3DRM_OK;
	}
	LPVOID GetAppData() override { return m_appData; }
	HRESULT SetName(const char* name) override
	{
		SDL_free(m_name);
		m_name = NULL;
		if (name) {
			m_name = SDL_strdup(name);
		}
		return D3DRM_OK;
	}
	HRESULT GetName(DWORD* size, char* name) override
	{
		if (!size) {
			return DDERR_INVALIDPARAMS;
		}
		const char* s = m_name ? m_name : "";
		size_t l = SDL_strlen(s);
		if (name) {
			SDL_strlcpy(name, s, *size);
		}
		else {
			*size = l + 1;
		}
		return D3DRM_OK;
	}

private:
	std::vector<std::pair<D3DRMOBJECTCALLBACK, void*>> m_callbacks;
	LPD3DRM_APPDATA m_appData = nullptr;
	char* m_name = nullptr;
};

struct Direct3DRMMeshImpl : public Direct3DRMObjectBase<IDirect3DRMMesh> {
	HRESULT Clone(int flags, GUID iid, void** object) override
	{
		if (SDL_memcmp(&iid, &IID_IDirect3DRMMesh, sizeof(GUID)) == 0) {
			*object = static_cast<IDirect3DRMMesh*>(new Direct3DRMMeshImpl);
			return DD_OK;
		}

		return DDERR_GENERIC;
	}
	HRESULT GetBox(D3DRMBOX* box) override { return DD_OK; }
	HRESULT AddGroup(int vertexCount, int faceCount, int vertexPerFace, void* faceBuffer, D3DRMGROUPINDEX* groupIndex)
		override
	{
		return DD_OK;
	}
	HRESULT GetGroup(
		int groupIndex,
		unsigned int* vertexCount,
		unsigned int* faceCount,
		unsigned int* vertexPerFace,
		DWORD* dataSize,
		unsigned int* data
	) override
	{
		return DD_OK;
	}
	HRESULT SetGroupColor(int groupIndex, D3DCOLOR color) override { return DD_OK; }
	HRESULT SetGroupColorRGB(int groupIndex, float r, float g, float b) override { return DD_OK; }
	HRESULT SetGroupMaterial(int groupIndex, IDirect3DRMMaterial* material) override { return DD_OK; }
	HRESULT SetGroupMapping(D3DRMGROUPINDEX groupIndex, D3DRMMAPPING mapping) override { return DD_OK; }
	HRESULT SetGroupQuality(int groupIndex, D3DRMRENDERQUALITY quality) override { return DD_OK; }
	HRESULT SetVertices(int groupIndex, int offset, int count, D3DRMVERTEX* vertices) override { return DD_OK; }
	HRESULT SetGroupTexture(int groupIndex, IDirect3DRMTexture* texture) override
	{
		m_groupTexture = texture;
		return DD_OK;
	}
	HRESULT GetGroupTexture(int groupIndex, LPDIRECT3DRMTEXTURE* texture) override
	{
		if (!m_groupTexture) {
			return DDERR_GENERIC;
		}
		m_groupTexture->AddRef();
		*texture = m_groupTexture;
		return DD_OK;
	}
	D3DRMMAPPING GetGroupMapping(int groupIndex) override { return D3DRMMAP_PERSPCORRECT; }
	D3DRMRENDERQUALITY GetGroupQuality(int groupIndex) override { return D3DRMRENDER_GOURAUD; }
	HRESULT GetGroupColor(D3DRMGROUPINDEX index) override { return DD_OK; }
	HRESULT GetVertices(int groupIndex, int startIndex, int count, D3DRMVERTEX* vertices) override { return DD_OK; }

private:
	IDirect3DRMTexture* m_groupTexture = nullptr;
};

struct Direct3DRMTextureImpl : public Direct3DRMObjectBase<IDirect3DRMTexture2> {
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override
	{
		if (SDL_memcmp(&riid, &IID_IDirect3DRMTexture2, sizeof(GUID)) == 0) {
			this->IUnknown::AddRef();
			*ppvObject = static_cast<IDirect3DRMTexture2*>(this);
			return DD_OK;
		}
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Direct3DRMTextureImpl does not implement guid");
		return E_NOINTERFACE;
	}
	HRESULT Changed(BOOL pixels, BOOL palette) override { return DD_OK; }
};

SDL_GPUShader* LoadShader(
	SDL_GPUDevice* device,
	const char* shaderFilename,
	Uint32 samplerCount,
	Uint32 uniformBufferCount,
	Uint32 storageBufferCount,
	Uint32 storageTextureCount
)
{
	const char* basePath = SDL_GetBasePath();
	if (!basePath) {
		SDL_Log("Failed to get base path.");
		return NULL;
	}

	// Detect shader stage based on filename extension
	SDL_GPUShaderStage stage;
	if (SDL_strstr(shaderFilename, ".vert")) {
		stage = SDL_GPU_SHADERSTAGE_VERTEX;
	}
	else if (SDL_strstr(shaderFilename, ".frag")) {
		stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
	}
	else {
		SDL_Log("Invalid shader stage: %s", shaderFilename);
		return NULL;
	}

	char fullPath[256];
	SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
	SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
	const char* entrypoint = "main";

	if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
		SDL_snprintf(fullPath, sizeof(fullPath), "%sShaders/Compiled/SPIRV/%s.spv", basePath, shaderFilename);
		format = SDL_GPU_SHADERFORMAT_SPIRV;
	}
	else if (formats & SDL_GPU_SHADERFORMAT_MSL) {
		SDL_snprintf(fullPath, sizeof(fullPath), "%sShaders/Compiled/MSL/%s.msl", basePath, shaderFilename);
		format = SDL_GPU_SHADERFORMAT_MSL;
		entrypoint = "main0";
	}
	else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
		SDL_snprintf(fullPath, sizeof(fullPath), "%sShaders/Compiled/DXIL/%s.dxil", basePath, shaderFilename);
		format = SDL_GPU_SHADERFORMAT_DXIL;
	}
	else {
		SDL_Log("Unsupported backend shader format.");
		return NULL;
	}

	size_t codeSize;
	void* code = SDL_LoadFile(fullPath, &codeSize);
	if (!code) {
		SDL_Log("Failed to load shader file: %s", fullPath);
		return NULL;
	}

	SDL_GPUShaderCreateInfo shaderInfo = {
		codeSize,
		(const Uint8*) code,
		entrypoint,
		format,
		stage,
		samplerCount,
		storageTextureCount,
		storageBufferCount,
		uniformBufferCount,
	};

	SDL_GPUShader* shader = SDL_CreateGPUShader(device, &shaderInfo);
	if (!shader) {
		SDL_Log("Shader creation failed.");
	}

	SDL_free(code);
	return shader;
}

SDL_GPUDevice* m_device;
SDL_GPUGraphicsPipeline* m_pipeline;
SDL_GPUTexture* m_transferTexture;
SDL_GPUTransferBuffer* m_downloadTransferBuffer;
SDL_GPUBuffer* VertexBuffer;
int m_vertexCount = 3;

struct Direct3DRMDevice2Impl : public Direct3DRMObjectBase<IDirect3DRMDevice2> {
	Direct3DRMDevice2Impl()
	{
		m_device = SDL_CreateGPUDevice(
			SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL,
			true,
			NULL
		);
		if (m_device == NULL) {
			SDL_Log("GPUCreateDevice failed");
			return;
		}
		if (DDWindow == NULL) {
			SDL_Log("CreateWindow failed: %s", SDL_GetError());
			return;
		}
		if (!SDL_ClaimWindowForGPUDevice(m_device, DDWindow)) {
			SDL_Log("GPUClaimWindow failed");
			return;
		}
		SDL_GPUShader* vertexShader = LoadShader(m_device, "PositionColor.vert", 0, 0, 0, 0);
		if (vertexShader == NULL) {
			SDL_Log("Failed to create vertex shader!");
			return;
		}
		SDL_GPUShader* fragmentShader = LoadShader(m_device, "SolidColor.frag", 0, 0, 0, 0);
		if (fragmentShader == NULL) {
			SDL_Log("Failed to create fragment shader!");
			return;
		}

		SDL_GPUColorTargetDescription colorTargets = {SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB};
		SDL_GPUVertexBufferDescription vertexBufferDescs[] = {
			{.slot = 0,
			 .pitch = sizeof(PositionColorVertex),
			 .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
			 .instance_step_rate = 0}
		};

		SDL_GPUVertexAttribute vertexAttrs[] = {
			{.location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = 0},
			{.location = 1,
			 .buffer_slot = 0,
			 .format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
			 .offset = sizeof(float) * 3}
		};

		SDL_GPUVertexInputState vertexInputState = {
			.vertex_buffer_descriptions = vertexBufferDescs,
			.num_vertex_buffers = 1,
			.vertex_attributes = vertexAttrs,
			.num_vertex_attributes = 2
		};

		SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo =
			{.vertex_shader = vertexShader,
			 .fragment_shader = fragmentShader,
			 .vertex_input_state = vertexInputState,
			 .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
			 .target_info = {
				 .color_target_descriptions = &colorTargets,
				 .num_color_targets = 1,
			 }};

		m_pipeline = SDL_CreateGPUGraphicsPipeline(m_device, &pipelineCreateInfo);
		if (m_pipeline == NULL) {
			SDL_Log("Failed to create fill pipeline!");
			return;
		}

		// Clean up shader resources
		SDL_ReleaseGPUShader(m_device, vertexShader);
		SDL_ReleaseGPUShader(m_device, fragmentShader);
		// Create the vertex buffer
		SDL_GPUBufferCreateInfo bufferCreateInfo = {
			.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
			.size = (Uint32) (sizeof(PositionColorVertex) * m_vertexCount)
		};

		VertexBuffer = SDL_CreateGPUBuffer(m_device, &bufferCreateInfo);

		SDL_GPUTransferBufferCreateInfo transferCreateInfo = {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = (Uint32) (sizeof(PositionColorVertex) * m_vertexCount)
		};

		SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(m_device, &transferCreateInfo);

		PositionColorVertex* transferData =
			(PositionColorVertex*) SDL_MapGPUTransferBuffer(m_device, transferBuffer, false);

		transferData[0] = (PositionColorVertex){-1, -1, 0, 255, 0, 0, 255};
		transferData[1] = (PositionColorVertex){1, -1, 0, 0, 0, 255, 255};
		transferData[2] = (PositionColorVertex){0, 1, 0, 0, 255, 0, 128};

		SDL_UnmapGPUTransferBuffer(m_device, transferBuffer);

		// Upload the transfer data to the vertex buffer
		SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(m_device);
		SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

		SDL_GPUTransferBufferLocation transferLocation = {.transfer_buffer = transferBuffer, .offset = 0};

		SDL_GPUBufferRegion bufferRegion =
			{.buffer = VertexBuffer, .offset = 0, .size = (Uint32) (sizeof(PositionColorVertex) * m_vertexCount)};

		SDL_UploadToGPUBuffer(copyPass, &transferLocation, &bufferRegion, false);

		SDL_EndGPUCopyPass(copyPass);
		SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
		SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);

		SDL_GPUTextureCreateInfo textureInfo = {};
		textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
		textureInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
		textureInfo.width = 640;
		textureInfo.height = 480;
		textureInfo.layer_count_or_depth = 1;
		textureInfo.num_levels = 1;
		textureInfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
		m_transferTexture = SDL_CreateGPUTexture(m_device, &textureInfo);
		if (m_transferTexture == NULL) {
			SDL_Log("Failed to create fill pipeline!");
			return;
		}
		SDL_GPUTransferBufferCreateInfo downloadTransferInfo = {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
			.size = 640 * 480 * 4
		};
		m_downloadTransferBuffer = SDL_CreateGPUTransferBuffer(m_device, &downloadTransferInfo);
		if (!m_downloadTransferBuffer) {
			return;
		}

		m_viewports = new Direct3DRMViewportArrayImpl;
		m_viewports->AddRef();
	}
	~Direct3DRMDevice2Impl() override
	{
		m_viewports->Release();

		SDL_ReleaseGPUGraphicsPipeline(m_device, m_pipeline);
		SDL_ReleaseWindowFromGPUDevice(m_device, DDWindow);
		SDL_ReleaseGPUBuffer(m_device, VertexBuffer);
		SDL_DestroyWindow(DDWindow);
		SDL_DestroyGPUDevice(m_device);
	}
	unsigned int GetWidth() override { return 640; }
	unsigned int GetHeight() override { return 480; }
	HRESULT SetBufferCount(int count) override { return DD_OK; }
	HRESULT GetBufferCount() override { return DD_OK; }
	HRESULT SetShades(unsigned int shadeCount) override { return DD_OK; }
	HRESULT GetShades() override { return DD_OK; }
	HRESULT SetQuality(D3DRMRENDERQUALITY quality) override { return DD_OK; }
	D3DRMRENDERQUALITY GetQuality() override { return D3DRMRENDERQUALITY::GOURAUD; }
	HRESULT SetDither(int dither) override { return DD_OK; }
	HRESULT GetDither() override { return DD_OK; }
	HRESULT SetTextureQuality(D3DRMTEXTUREQUALITY quality) override { return DD_OK; }
	D3DRMTEXTUREQUALITY GetTextureQuality() override { return D3DRMTEXTUREQUALITY::LINEAR; }
	HRESULT SetRenderMode(D3DRMRENDERMODE mode) override { return DD_OK; }
	D3DRMRENDERMODE GetRenderMode() override { return D3DRMRENDERMODE::BLENDEDTRANSPARENCY; }
	HRESULT Update() override { return DD_OK; }
	HRESULT AddViewport(IDirect3DRMViewport* viewport) override { return m_viewports->AddElement(viewport); }
	HRESULT GetViewports(IDirect3DRMViewportArray** ppViewportArray) override
	{
		*ppViewportArray = m_viewports;
		return DD_OK;
	}

private:
	IDirect3DRMViewportArray* m_viewports;
};

struct Direct3DRMFrameImpl : public Direct3DRMObjectBase<IDirect3DRMFrame2> {
	Direct3DRMFrameImpl()
	{
		m_children = new Direct3DRMFrameArrayImpl;
		m_children->AddRef();
		m_lights = new Direct3DRMLightArrayImpl;
		m_lights->AddRef();
		m_visuals = new Direct3DRMVisualArrayImpl;
		m_visuals->AddRef();
	}
	~Direct3DRMFrameImpl() override
	{
		m_children->Release();
		m_lights->Release();
		m_visuals->Release();
		if (m_texture) {
			m_texture->Release();
		}
	}
	HRESULT AddChild(IDirect3DRMFrame* child) override { return m_children->AddElement(child); }
	HRESULT DeleteChild(IDirect3DRMFrame* child) override { return m_children->DeleteElement(child); }
	HRESULT SetSceneBackgroundRGB(float r, float g, float b) override { return DD_OK; }
	HRESULT AddLight(IDirect3DRMLight* light) override { return m_lights->AddElement(light); }
	HRESULT GetLights(IDirect3DRMLightArray** lightArray) override
	{
		*lightArray = m_lights;
		m_lights->AddRef();
		return DD_OK;
	}
	HRESULT AddTransform(D3DRMCOMBINETYPE combine, D3DRMMATRIX4D matrix) override { return DD_OK; }
	HRESULT GetPosition(int index, D3DVECTOR* position) override { return DD_OK; }
	HRESULT AddVisual(IDirect3DRMVisual* visual) override { return m_visuals->AddElement(visual); }
	HRESULT DeleteVisual(IDirect3DRMVisual* visual) override { return m_visuals->DeleteElement(visual); }
	HRESULT AddVisual(IDirect3DRMMesh* visual) override { return m_visuals->AddElement(visual); }
	HRESULT DeleteVisual(IDirect3DRMMesh* visual) override { return m_visuals->DeleteElement(visual); }
	HRESULT AddVisual(IDirect3DRMFrame* visual) override { return m_visuals->AddElement(visual); }
	HRESULT DeleteVisual(IDirect3DRMFrame* visual) override { return m_visuals->DeleteElement(visual); }
	HRESULT GetVisuals(IDirect3DRMVisualArray** visuals) override
	{
		*visuals = m_visuals;
		m_visuals->AddRef();
		return DD_OK;
	}
	HRESULT SetTexture(IDirect3DRMTexture* texture) override
	{
		if (m_texture) {
			m_texture->Release();
		}
		m_texture = texture;
		m_texture->AddRef();
		return DD_OK;
	}
	HRESULT GetTexture(IDirect3DRMTexture** texture) override
	{
		if (!m_texture) {
			return DDERR_GENERIC;
		}
		*texture = m_texture;
		m_texture->AddRef();
		return DD_OK;
	}
	HRESULT SetColor(float r, float g, float b, float a) override { return DD_OK; }
	HRESULT SetColor(D3DCOLOR) override { return DD_OK; }
	HRESULT SetColorRGB(float r, float g, float b) override { return DD_OK; }
	HRESULT SetMaterialMode(D3DRMMATERIALMODE mode) override { return DD_OK; }
	HRESULT GetChildren(IDirect3DRMFrameArray** children) override
	{
		*children = m_children;
		m_children->AddRef();
		return DD_OK;
	}

private:
	IDirect3DRMFrameArray* m_children;
	IDirect3DRMLightArray* m_lights;
	IDirect3DRMVisualArray* m_visuals;
	IDirect3DRMTexture* m_texture = nullptr;
};

struct Direct3DRMViewportImpl : public Direct3DRMObjectBase<IDirect3DRMViewport> {
	HRESULT Render(IDirect3DRMFrame* group) override
	{
		if (!m_transferTexture || !DDBackBuffer) {
			return DDERR_GENERIC;
		}
		SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(m_device);
		if (cmdbuf == NULL) {
			return DDERR_GENERIC;
		}

		// Render the graphics
		SDL_GPUColorTargetInfo colorTargetInfo = {};
		colorTargetInfo.texture = m_transferTexture;
		// Make the render target transparent so we can combine it with the back buffer
		colorTargetInfo.clear_color = {0, 0, 0, 0};
		colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;

		SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, NULL);
		SDL_BindGPUGraphicsPipeline(renderPass, m_pipeline);
		SDL_GPUBufferBinding vertexBufferBinding = {.buffer = VertexBuffer, .offset = 0};
		SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBufferBinding, 1);
		SDL_DrawGPUPrimitives(renderPass, m_vertexCount, 1, 0, 0);
		SDL_EndGPURenderPass(renderPass);

		// Download rendered image
		SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdbuf);
		SDL_GPUTextureRegion region = {};
		region.texture = m_transferTexture;
		region.w = DDBackBuffer->w;
		region.h = DDBackBuffer->h;
		region.d = 1;
		SDL_GPUTextureTransferInfo transferInfo = {};
		transferInfo.transfer_buffer = m_downloadTransferBuffer;
		transferInfo.offset = 0;
		SDL_DownloadFromGPUTexture(copyPass, &region, &transferInfo);
		SDL_EndGPUCopyPass(copyPass);
		SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmdbuf);
		if (!cmdbuf || !SDL_WaitForGPUFences(m_device, true, &fence, 1)) {
			return DDERR_GENERIC;
		}
		SDL_ReleaseGPUFence(m_device, fence);
		void* downloadedData = SDL_MapGPUTransferBuffer(m_device, m_downloadTransferBuffer, false);
		if (!downloadedData) {
			return DDERR_GENERIC;
		}
		SDL_Surface* renderedImage = SDL_CreateSurfaceFrom(
			DDBackBuffer->w,
			DDBackBuffer->h,
			SDL_PIXELFORMAT_ABGR8888,
			downloadedData,
			DDBackBuffer->w * 4
		);
		if (!renderedImage) {
			return DDERR_GENERIC;
		}

		// Blit the render back to our backbuffer
		SDL_Rect srcRect{0, 0, DDBackBuffer->w, DDBackBuffer->h};

		if (renderedImage->format == DDBackBuffer->format) {
			// No conversion needed
			SDL_BlitSurface(renderedImage, &srcRect, DDBackBuffer, &srcRect);
			SDL_DestroySurface(renderedImage);
			return DD_OK;
		}

		const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(DDBackBuffer->format);
		if (details->Amask != 0) {
			// Backbuffer supports transparnacy
			SDL_Surface* convertedRender = SDL_ConvertSurface(renderedImage, DDBackBuffer->format);
			SDL_DestroySurface(renderedImage);
			SDL_BlitSurface(convertedRender, &srcRect, DDBackBuffer, &srcRect);
			SDL_DestroySurface(convertedRender);
			return DD_OK;
		}

		// Convert backbuffer to a format that supports transparancy
		SDL_Surface* tempBackbuffer = SDL_ConvertSurface(DDBackBuffer, renderedImage->format);
		SDL_BlitSurface(renderedImage, &srcRect, tempBackbuffer, &srcRect);
		SDL_DestroySurface(renderedImage);
		// Then convert the result back to the backbuffer format and write it back
		SDL_Surface* newBackBuffer = SDL_ConvertSurface(tempBackbuffer, DDBackBuffer->format);
		SDL_DestroySurface(tempBackbuffer);
		SDL_BlitSurface(newBackBuffer, &srcRect, DDBackBuffer, &srcRect);
		SDL_DestroySurface(newBackBuffer);
		return DD_OK;
	}
	HRESULT ForceUpdate(int x, int y, int w, int h) override { return DD_OK; }
	HRESULT Clear() override { return DD_OK; }
	HRESULT SetCamera(IDirect3DRMFrame* camera) override
	{
		m_camera = camera;
		return DD_OK;
	}
	HRESULT GetCamera(IDirect3DRMFrame** camera) override
	{
		*camera = m_camera;
		return DD_OK;
	}
	HRESULT SetProjection(D3DRMPROJECTIONTYPE type) override { return DD_OK; }
	D3DRMPROJECTIONTYPE GetProjection() override { return D3DRMPROJECTIONTYPE::PERSPECTIVE; }
	HRESULT SetFront(D3DVALUE z) override { return DD_OK; }
	D3DVALUE GetFront() override { return 0; }
	HRESULT SetBack(D3DVALUE z) override { return DD_OK; }
	D3DVALUE GetBack() override { return 0; }
	HRESULT SetField(D3DVALUE field) override { return DD_OK; }
	D3DVALUE GetField() override { return 0; }
	int GetWidth() override { return 640; }
	int GetHeight() override { return 480; }
	HRESULT Transform(D3DRMVECTOR4D* screen, D3DVECTOR* world) override { return DD_OK; }
	HRESULT InverseTransform(D3DVECTOR* world, D3DRMVECTOR4D* screen) override { return DD_OK; }
	HRESULT Pick(float x, float y, LPDIRECT3DRMPICKEDARRAY* pickedArray) override { return DD_OK; }

private:
	IDirect3DRMFrame* m_camera = nullptr;
};

struct Direct3DRMLightImpl : public Direct3DRMObjectBase<IDirect3DRMLight> {
	HRESULT SetColorRGB(float r, float g, float b) override { return DD_OK; }
};

struct Direct3DRMMaterialImpl : public Direct3DRMObjectBase<IDirect3DRMMaterial> {};

struct Direct3DRMImpl : virtual public IDirect3DRM2 {
	// IUnknown interface
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override
	{
		if (SDL_memcmp(&riid, &IID_IDirect3DRM2, sizeof(GUID)) == 0) {
			this->IUnknown::AddRef();
			*ppvObject = static_cast<IDirect3DRM2*>(this);
			return DD_OK;
		}
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DirectDrawImpl does not implement guid");
		return E_NOINTERFACE;
	}
	// IDirect3DRM interface
	HRESULT CreateDeviceFromD3D(const IDirect3D2* d3d, IDirect3DDevice2* d3dDevice, IDirect3DRMDevice2** outDevice)
		override
	{
		*outDevice = static_cast<IDirect3DRMDevice2*>(new Direct3DRMDevice2Impl);
		return DD_OK;
	}
	HRESULT CreateDeviceFromSurface(
		const GUID* guid,
		IDirectDraw* dd,
		IDirectDrawSurface* surface,
		IDirect3DRMDevice2** outDevice
	) override
	{
		*outDevice = static_cast<IDirect3DRMDevice2*>(new Direct3DRMDevice2Impl);
		return DD_OK;
	}
	HRESULT CreateTexture(D3DRMIMAGE* image, IDirect3DRMTexture2** outTexture) override
	{
		*outTexture = static_cast<IDirect3DRMTexture2*>(new Direct3DRMTextureImpl);
		return DD_OK;
	}
	HRESULT CreateTextureFromSurface(LPDIRECTDRAWSURFACE surface, IDirect3DRMTexture2** outTexture) override
	{
		*outTexture = static_cast<IDirect3DRMTexture2*>(new Direct3DRMTextureImpl);
		return DD_OK;
	}
	HRESULT CreateMesh(IDirect3DRMMesh** outMesh) override
	{
		*outMesh = static_cast<IDirect3DRMMesh*>(new Direct3DRMMeshImpl);
		return DD_OK;
	}
	HRESULT CreateMaterial(D3DVAL power, IDirect3DRMMaterial** outMaterial) override
	{
		*outMaterial = static_cast<IDirect3DRMMaterial*>(new Direct3DRMMaterialImpl);
		return DD_OK;
	}
	HRESULT CreateLightRGB(D3DRMLIGHTTYPE type, D3DVAL r, D3DVAL g, D3DVAL b, IDirect3DRMLight** outLight) override
	{
		*outLight = static_cast<IDirect3DRMLight*>(new Direct3DRMLightImpl);
		return DD_OK;
	}
	HRESULT CreateFrame(IDirect3DRMFrame* parent, IDirect3DRMFrame2** outFrame) override
	{
		*outFrame = static_cast<IDirect3DRMFrame2*>(new Direct3DRMFrameImpl);
		return DD_OK;
	}
	HRESULT CreateViewport(
		IDirect3DRMDevice2* device,
		IDirect3DRMFrame* camera,
		int x,
		int y,
		int width,
		int height,
		IDirect3DRMViewport** outViewport
	) override
	{
		*outViewport = static_cast<IDirect3DRMViewport*>(new Direct3DRMViewportImpl);
		device->AddViewport(*outViewport);
		return DD_OK;
	}
	HRESULT SetDefaultTextureShades(unsigned int count) override { return DD_OK; }
	HRESULT SetDefaultTextureColors(unsigned int count) override { return DD_OK; }
};

HRESULT WINAPI Direct3DRMCreate(IDirect3DRM** direct3DRM)
{
	*direct3DRM = new Direct3DRMImpl;
	return DD_OK;
}

D3DCOLOR D3DRMCreateColorRGBA(D3DVALUE red, D3DVALUE green, D3DVALUE blue, D3DVALUE alpha)
{
	return 0;
}
