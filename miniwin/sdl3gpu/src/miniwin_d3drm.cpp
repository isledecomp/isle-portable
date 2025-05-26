#include "miniwin_d3drm.h"

#include "ShaderIndex.h"
#include "miniwin_d3drm_sdl3gpu.h"
#include "miniwin_d3drmdevice_sdl3gpu.h"
#include "miniwin_d3drmframe_sdl3gpu.h"
#include "miniwin_d3drmlight_sdl3gpu.h"
#include "miniwin_d3drmmesh_sdl3gpu.h"
#include "miniwin_d3drmobject_sdl3gpu.h"
#include "miniwin_d3drmtexture_sdl3gpu.h"
#include "miniwin_d3drmviewport_sdl3gpu.h"
#include "miniwin_ddsurface_sdl3gpu.h"
#include "miniwin_p.h"

#include <SDL3/SDL.h>

Direct3DRMPickedArray_SDL3GPUImpl::Direct3DRMPickedArray_SDL3GPUImpl(const PickRecord* inputPicks, size_t count)
{
	picks.reserve(count);
	for (size_t i = 0; i < count; ++i) {
		const PickRecord& pick = inputPicks[i];
		if (pick.visual) {
			pick.visual->AddRef();
		}
		if (pick.frameArray) {
			pick.frameArray->AddRef();
		}
		picks.push_back(pick);
	}
}

Direct3DRMPickedArray_SDL3GPUImpl::~Direct3DRMPickedArray_SDL3GPUImpl()
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

DWORD Direct3DRMPickedArray_SDL3GPUImpl::GetSize()
{
	return static_cast<DWORD>(picks.size());
}

HRESULT Direct3DRMPickedArray_SDL3GPUImpl::GetPick(
	DWORD index,
	IDirect3DRMVisual** visual,
	IDirect3DRMFrameArray** frameArray,
	D3DRMPICKDESC* desc
)
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

struct Direct3DRMWinDevice_SDL3GPUImpl : public IDirect3DRMWinDevice {
	HRESULT Activate() override
	{
		MINIWIN_NOT_IMPLEMENTED();
		return DD_OK;
	}
	HRESULT Paint() override
	{
		MINIWIN_NOT_IMPLEMENTED();
		return DD_OK;
	}
	void HandleActivate(WORD wParam) override { MINIWIN_NOT_IMPLEMENTED(); }
	void HandlePaint(void* p_dc) override { MINIWIN_NOT_IMPLEMENTED(); }
};

struct Direct3DRMMaterial_SDL3GPUImpl : public Direct3DRMObjectBase_SDL3GPUImpl<IDirect3DRMMaterial> {
	Direct3DRMMaterial_SDL3GPUImpl(D3DVALUE power) : m_power(power) {}
	D3DVALUE GetPower() override { return m_power; }

private:
	D3DVALUE m_power;
};

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

HRESULT Direct3DRM_SDL3GPUImpl::QueryInterface(const GUID& riid, void** ppvObject)
{
	if (SDL_memcmp(&riid, &IID_IDirect3DRM2, sizeof(GUID)) == 0) {
		this->IUnknown::AddRef();
		*ppvObject = static_cast<IDirect3DRM2*>(this);
		return DD_OK;
	}
	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Direct3DRM_SDL3GPUImpl does not implement guid");
	return E_NOINTERFACE;
}

HRESULT Direct3DRM_SDL3GPUImpl::CreateDevice(IDirect3DRMDevice2** outDevice, DWORD width, DWORD height)
{
	SDL_GPUDevice* device = SDL_CreateGPUDevice(
		SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL,
		true,
		NULL
	);
	if (device == NULL) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_CreateGPUDevice failed (%s)", SDL_GetError());
		return DDERR_GENERIC;
	}
	if (DDWindow_SDL3GPU == NULL) {
		return DDERR_GENERIC;
	}
	if (!SDL_ClaimWindowForGPUDevice(device, DDWindow_SDL3GPU)) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_ClaimWindowForGPUDevice failed (%s)", SDL_GetError());
		return DDERR_GENERIC;
	}

	*outDevice = static_cast<IDirect3DRMDevice2*>(new Direct3DRMDevice2_SDL3GPUImpl(width, height, device));
	return DD_OK;
}

HRESULT Direct3DRM_SDL3GPUImpl::CreateDeviceFromD3D(
	const IDirect3D2* d3d,
	IDirect3DDevice2* d3dDevice,
	IDirect3DRMDevice2** outDevice
)
{
	MINIWIN_NOT_IMPLEMENTED();
	return CreateDevice(outDevice, 640, 480);
}

HRESULT Direct3DRM_SDL3GPUImpl::CreateDeviceFromSurface(
	const GUID* guid,
	IDirectDraw* dd,
	IDirectDrawSurface* surface,
	IDirect3DRMDevice2** outDevice
)
{
	MINIWIN_NOT_IMPLEMENTED(); // Respect the chosen GUID
	DDSURFACEDESC DDSDesc;
	DDSDesc.dwSize = sizeof(DDSURFACEDESC);
	surface->GetSurfaceDesc(&DDSDesc);
	return CreateDevice(outDevice, DDSDesc.dwWidth, DDSDesc.dwHeight);
}

HRESULT Direct3DRM_SDL3GPUImpl::CreateTexture(D3DRMIMAGE* image, IDirect3DRMTexture2** outTexture)
{
	MINIWIN_NOT_IMPLEMENTED();
	*outTexture = static_cast<IDirect3DRMTexture2*>(new Direct3DRMTexture_SDL3GPUImpl);
	return DD_OK;
}

HRESULT Direct3DRM_SDL3GPUImpl::CreateTextureFromSurface(LPDIRECTDRAWSURFACE surface, IDirect3DRMTexture2** outTexture)

{
	MINIWIN_NOT_IMPLEMENTED();
	*outTexture = static_cast<IDirect3DRMTexture2*>(new Direct3DRMTexture_SDL3GPUImpl);
	return DD_OK;
}

HRESULT Direct3DRM_SDL3GPUImpl::CreateMesh(IDirect3DRMMesh** outMesh)
{
	*outMesh = static_cast<IDirect3DRMMesh*>(new Direct3DRMMesh_SDL3GPUImpl);
	return DD_OK;
}

HRESULT Direct3DRM_SDL3GPUImpl::CreateMaterial(D3DVAL power, IDirect3DRMMaterial** outMaterial)
{
	*outMaterial = static_cast<IDirect3DRMMaterial*>(new Direct3DRMMaterial_SDL3GPUImpl(power));
	return DD_OK;
}

HRESULT Direct3DRM_SDL3GPUImpl::CreateLightRGB(
	D3DRMLIGHTTYPE type,
	D3DVAL r,
	D3DVAL g,
	D3DVAL b,
	IDirect3DRMLight** outLight
)
{
	*outLight = static_cast<IDirect3DRMLight*>(new Direct3DRMLight_SDL3GPUImpl(r, g, b));
	return DD_OK;
}

HRESULT Direct3DRM_SDL3GPUImpl::CreateFrame(IDirect3DRMFrame* parent, IDirect3DRMFrame2** outFrame)
{
	auto parentImpl = static_cast<Direct3DRMFrame_SDL3GPUImpl*>(parent);
	*outFrame = static_cast<IDirect3DRMFrame2*>(new Direct3DRMFrame_SDL3GPUImpl{parentImpl});
	return DD_OK;
}

HRESULT Direct3DRM_SDL3GPUImpl::CreateViewport(
	IDirect3DRMDevice2* iDevice,
	IDirect3DRMFrame* camera,
	int x,
	int y,
	int width,
	int height,
	IDirect3DRMViewport** outViewport
)
{
	auto device = static_cast<Direct3DRMDevice2_SDL3GPUImpl*>(iDevice);

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

	auto* viewport = new Direct3DRMViewport_SDL3GPUImpl(
		width,
		height,
		device->m_device,
		transferTexture,
		downloadTransferBuffer,
		pipeline
	);
	if (camera) {
		viewport->SetCamera(camera);
	}
	*outViewport = static_cast<IDirect3DRMViewport*>(viewport);
	device->AddViewport(*outViewport);
	return DD_OK;
}

HRESULT Direct3DRM_SDL3GPUImpl::SetDefaultTextureShades(DWORD count)
{
	return DD_OK;
}

HRESULT Direct3DRM_SDL3GPUImpl::SetDefaultTextureColors(DWORD count)
{
	return DD_OK;
}
