#include "miniwin_d3drm.h"

#include "ShaderIndex.h"
#include "miniwin_d3drm_p.h"
#include "miniwin_d3drmframe_p.h"
#include "miniwin_d3drmlight_p.h"
#include "miniwin_d3drmmesh_p.h"
#include "miniwin_d3drmobject_p.h"
#include "miniwin_d3drmviewport_p.h"
#include "miniwin_ddsurface_p.h"
#include "miniwin_p.h"

#include <SDL3/SDL.h>

#define RGBA_MAKE(r, g, b, a) ((D3DCOLOR) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b)))

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
struct Direct3DRMMaterialImpl : public Direct3DRMObjectBase<IDirect3DRMMaterial> {};

SDL_GPUGraphicsPipeline* InitializeGraphicsPipeline(SDL_GPUDevice* device)
{
	const SDL_GPUShaderCreateInfo* vertexCreateInfo =
		GetVertexShaderCode(VertexShaderId::PositionColor, SDL_GetGPUShaderFormats(device));
	if (!vertexCreateInfo) {
		return nullptr;
	}
	SDL_GPUShader* vertexShader = SDL_CreateGPUShader(device, vertexCreateInfo);
	if (!vertexShader) {
		return nullptr;
	}

	const SDL_GPUShaderCreateInfo* fragmentCreateInfo =
		GetFragmentShaderCode(FragmentShaderId::SolidColor, SDL_GetGPUShaderFormats(device));
	if (!fragmentCreateInfo) {
		return nullptr;
	}
	SDL_GPUShader* fragmentShader = SDL_CreateGPUShader(device, fragmentCreateInfo);
	if (!fragmentShader) {
		return nullptr;
	}

	SDL_GPUVertexBufferDescription vertexBufferDescs[1] = {};
	vertexBufferDescs[0].slot = 0;
	vertexBufferDescs[0].pitch = sizeof(PositionColorVertex);
	vertexBufferDescs[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
	vertexBufferDescs[0].instance_step_rate = 0;

	SDL_GPUVertexAttribute vertexAttrs[2] = {};
	vertexAttrs[0].location = 0;
	vertexAttrs[0].buffer_slot = 0;
	vertexAttrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	vertexAttrs[0].offset = 0;

	vertexAttrs[1].location = 1;
	vertexAttrs[1].buffer_slot = 0;
	vertexAttrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
	vertexAttrs[1].offset = sizeof(float) * 3;

	SDL_GPUVertexInputState vertexInputState = {};
	vertexInputState.vertex_buffer_descriptions = vertexBufferDescs;
	vertexInputState.num_vertex_buffers = 1;
	vertexInputState.vertex_attributes = vertexAttrs;
	vertexInputState.num_vertex_attributes = 2;

	SDL_GPUColorTargetDescription colorTargets = {};
	colorTargets.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;

	SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.vertex_shader = vertexShader;
	pipelineCreateInfo.fragment_shader = fragmentShader;
	pipelineCreateInfo.vertex_input_state = vertexInputState;
	pipelineCreateInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	pipelineCreateInfo.target_info.color_target_descriptions = &colorTargets;
	pipelineCreateInfo.target_info.num_color_targets = 1;

	SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineCreateInfo);
	// Clean up shader resources
	SDL_ReleaseGPUShader(device, vertexShader);
	SDL_ReleaseGPUShader(device, fragmentShader);

	return pipeline;
}

struct Direct3DRMImpl : virtual public IDirect3DRM2 {
	// IUnknown interface
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override
	{
		if (SDL_memcmp(&riid, &IID_IDirect3DRM2, sizeof(GUID)) == 0) {
			this->IUnknown::AddRef();
			*ppvObject = static_cast<IDirect3DRM2*>(this);
			return DD_OK;
		}
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Direct3DRMImpl does not implement guid");
		return E_NOINTERFACE;
	}
	// IDirect3DRM interface
	HRESULT CreateDevice(IDirect3DRMDevice2** outDevice, DWORD width, DWORD height)
	{
		SDL_GPUDevice* device = SDL_CreateGPUDevice(
			SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL,
			true,
			NULL
		);
		if (device == NULL) {
			return DDERR_GENERIC;
		}
		if (DDWindow == NULL) {
			return DDERR_GENERIC;
		}
		if (!SDL_ClaimWindowForGPUDevice(device, DDWindow)) {
			return DDERR_GENERIC;
		}

		*outDevice = static_cast<IDirect3DRMDevice2*>(new Direct3DRMDevice2Impl(width, height, device));
		return DD_OK;
	}
	HRESULT CreateDeviceFromD3D(const IDirect3D2* d3d, IDirect3DDevice2* d3dDevice, IDirect3DRMDevice2** outDevice)
		override
	{
		return CreateDevice(outDevice, 640, 480);
	}
	HRESULT CreateDeviceFromSurface(
		const GUID* guid,
		IDirectDraw* dd,
		IDirectDrawSurface* surface,
		IDirect3DRMDevice2** outDevice
	) override
	{
		DDSURFACEDESC DDSDesc;
		DDSDesc.dwSize = sizeof(DDSURFACEDESC);
		surface->GetSurfaceDesc(&DDSDesc);
		return CreateDevice(outDevice, DDSDesc.dwWidth, DDSDesc.dwHeight);
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
		IDirect3DRMDevice2* iDevice,
		IDirect3DRMFrame* camera,
		int x,
		int y,
		int width,
		int height,
		IDirect3DRMViewport** outViewport
	) override
	{
		auto device = static_cast<Direct3DRMDevice2Impl*>(iDevice);

		SDL_GPUGraphicsPipeline* pipeline = InitializeGraphicsPipeline(device->m_device);
		if (!pipeline) {
			return DDERR_GENERIC;
		}

		SDL_GPUTextureCreateInfo textureInfo = {};
		textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
		textureInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
		textureInfo.width = width;
		textureInfo.height = height;
		textureInfo.layer_count_or_depth = 1;
		textureInfo.num_levels = 1;
		textureInfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
		SDL_GPUTexture* transferTexture = SDL_CreateGPUTexture(device->m_device, &textureInfo);
		if (!transferTexture) {
			return DDERR_GENERIC;
		}

		// Setup texture GPU-to-CPU transfer
		SDL_GPUTransferBufferCreateInfo downloadTransferInfo = {};
		downloadTransferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
		downloadTransferInfo.size = static_cast<Uint32>(width * height * 4);
		SDL_GPUTransferBuffer* downloadTransferBuffer =
			SDL_CreateGPUTransferBuffer(device->m_device, &downloadTransferInfo);
		if (!downloadTransferBuffer) {
			return DDERR_GENERIC;
		}

		*outViewport = static_cast<IDirect3DRMViewport*>(new Direct3DRMViewportImpl(
			width,
			height,
			device->m_device,
			transferTexture,
			downloadTransferBuffer,
			pipeline
		));
		device->AddViewport(*outViewport);
		return DD_OK;
	}
	HRESULT SetDefaultTextureShades(DWORD count) override { return DD_OK; }
	HRESULT SetDefaultTextureColors(DWORD count) override { return DD_OK; }
};

HRESULT WINAPI Direct3DRMCreate(IDirect3DRM** direct3DRM)
{
	*direct3DRM = new Direct3DRMImpl;
	return DD_OK;
}

D3DCOLOR D3DRMCreateColorRGBA(D3DVALUE red, D3DVALUE green, D3DVALUE blue, D3DVALUE alpha)
{
	return RGBA_MAKE((int) (255.f * red), (int) (255.f * green), (int) (255.f * blue), (int) (255.f * alpha));
}
