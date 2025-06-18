#include "actual.h"

#include "structs.h"

#include <SDL3/SDL.h>
#include <d3d9.h>
#include <vector>
#include <windows.h>

// Global D3D9 state
static LPDIRECT3D9 g_d3d;
static LPDIRECT3DDEVICE9 g_device;
static HWND g_hwnd;
static LPDIRECT3DTEXTURE9 g_renderTargetTex;
static LPDIRECT3DSURFACE9 g_renderTargetSurf;
static LPDIRECT3DSURFACE9 g_oldRenderTarget;
static int m_width;
static int m_height;
static std::vector<BridgeSceneLight> m_lights;
static Matrix4x4 m_projection;

bool Actual_Initialize(void* hwnd, int width, int height)
{
	g_hwnd = (HWND) hwnd;
	m_width = width;
	m_height = height;
	g_d3d = Direct3DCreate9(D3D_SDK_VERSION);
	if (!g_d3d) {
		return false;
	}

	D3DPRESENT_PARAMETERS pp = {};
	pp.Windowed = TRUE;
	pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	pp.hDeviceWindow = g_hwnd;
	pp.BackBufferFormat = D3DFMT_A8R8G8B8;
	pp.BackBufferWidth = width;
	pp.BackBufferHeight = height;
	pp.EnableAutoDepthStencil = TRUE;
	pp.AutoDepthStencilFormat = D3DFMT_D24S8;

	HRESULT hr = g_d3d->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		g_hwnd,
		D3DCREATE_SOFTWARE_VERTEXPROCESSING,
		&pp,
		&g_device
	);
	if (FAILED(hr)) {
		g_d3d->Release();
		return false;
	}

	hr = g_device->CreateTexture(
		width,
		height,
		1,
		D3DUSAGE_RENDERTARGET,
		D3DFMT_A8R8G8B8,
		D3DPOOL_DEFAULT,
		&g_renderTargetTex,
		nullptr
	);
	if (FAILED(hr)) {
		g_device->Release();
		g_d3d->Release();
		return false;
	}

	hr = g_renderTargetTex->GetSurfaceLevel(0, &g_renderTargetSurf);
	if (FAILED(hr)) {
		g_renderTargetTex->Release();
		g_device->Release();
		g_d3d->Release();
		return false;
	}

	g_device->SetFVF(D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1);

	return true;
}

void Actual_Shutdown()
{
	if (g_renderTargetSurf) {
		g_renderTargetSurf->Release();
		g_renderTargetSurf = nullptr;
	}
	if (g_renderTargetTex) {
		g_renderTargetTex->Release();
		g_renderTargetTex = nullptr;
	}
	if (g_device) {
		g_device->Release();
		g_device = nullptr;
	}
	if (g_d3d) {
		g_d3d->Release();
		g_d3d = nullptr;
	}
}

void Actual_PushLights(const BridgeSceneLight* lightsArray, size_t count)
{
	m_lights.assign(lightsArray, lightsArray + count);
}

void Actual_SetProjection(const Matrix4x4* projection, float front, float back)
{
	memcpy(&m_projection, projection, sizeof(Matrix4x4));
}

IDirect3DTexture9* UploadSurfaceToD3DTexture(SDL_Surface* surface)
{
	IDirect3DTexture9* texture;

	HRESULT hr =
		g_device->CreateTexture(surface->w, surface->h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, nullptr);

	if (FAILED(hr)) {
		return nullptr;
	}

	SDL_Surface* conv = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ARGB8888);
	if (!conv) {
		texture->Release();
		return nullptr;
	}

	D3DLOCKED_RECT lockedRect;
	texture->LockRect(0, &lockedRect, nullptr, 0);

	for (int y = 0; y < conv->h; ++y) {
		memcpy(
			(uint8_t*) lockedRect.pBits + y * lockedRect.Pitch,
			(uint8_t*) conv->pixels + y * conv->pitch,
			conv->w * 4
		);
	}

	texture->UnlockRect(0);
	SDL_DestroySurface(conv);

	return texture;
}

void ReleaseD3DTexture(IDirect3DTexture9* texture)
{
	texture->Release();
}

void ReleaseD3DVertexBuffer(IDirect3DVertexBuffer9* buffer)
{
	buffer->Release();
}

void ReleaseD3DIndexBuffer(IDirect3DIndexBuffer9* buffer)
{
	buffer->Release();
}

void UploadMeshBuffers(
	const void* vertices,
	int vertexSize,
	const uint16_t* indices,
	int indexSize,
	D3D9MeshCacheEntry& cache
)
{
	g_device->CreateVertexBuffer(
		vertexSize,
		0,
		D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1,
		D3DPOOL_MANAGED,
		&cache.vbo,
		nullptr
	);
	void* vbData;
	cache.vbo->Lock(0, 0, &vbData, 0);
	memcpy(vbData, vertices, vertexSize);
	cache.vbo->Unlock();

	g_device->CreateIndexBuffer(indexSize, 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &cache.ibo, nullptr);
	void* ibData;
	cache.ibo->Lock(0, 0, &ibData, 0);
	memcpy(ibData, indices, indexSize);
	cache.ibo->Unlock();
}

constexpr float PI = 3.14159265358979323846f;

void SetupLights()
{
	g_device->SetRenderState(D3DRS_LIGHTING, TRUE);

	for (DWORD i = 0; i < 8; ++i) {
		g_device->LightEnable(i, FALSE);
	}

	DWORD lightIdx = 0;
	for (const auto& l : m_lights) {
		if (lightIdx >= 8) {
			break;
		}

		const FColor c = l.color;

		if (l.directional != 1.0f && l.positional != 1.0f) {
			g_device->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_COLORVALUE(c.r, c.g, c.b, 1.0f));
			continue;
		}

		D3DLIGHT9 light = {};
		light.Type = (l.directional == 1.0f) ? D3DLIGHT_DIRECTIONAL : D3DLIGHT_POINT;

		light.Ambient = {0, 0, 0, 0};
		light.Diffuse = {c.r, c.g, c.b, c.a};

		if (light.Type == D3DLIGHT_DIRECTIONAL) {
			light.Direction.x = -l.direction.x;
			light.Direction.y = -l.direction.y;
			light.Direction.z = -l.direction.z;
			light.Specular = {0, 0, 0, 0};
		}
		else if (light.Type == D3DLIGHT_POINT) {
			light.Specular = {1, 1, 1, 1};
			light.Position.x = l.position.x;
			light.Position.y = l.position.y;
			light.Position.z = l.position.z;
			light.Range = 1000.0f;
			light.Attenuation0 = 1.0f;
			light.Attenuation1 = 0.0f;
			light.Attenuation2 = 0.0f;
		}

		light.Falloff = 1.0f;
		light.Phi = PI;
		light.Theta = PI / 2;

		if (SUCCEEDED(g_device->SetLight(lightIdx, &light))) {
			g_device->LightEnable(lightIdx, TRUE);
		}
		++lightIdx;
	}
}

uint32_t Actual_BeginFrame()
{
	g_device->GetRenderTarget(0, &g_oldRenderTarget);
	g_device->SetRenderTarget(0, g_renderTargetSurf);

	g_device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);

	g_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
	g_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	g_device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);

	g_device->BeginScene();

	SetupLights();

	return D3D_OK;
}

void Actual_EnableTransparency()
{
	g_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	g_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	g_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	g_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
}

D3DMATRIX ToD3DMATRIX(const Matrix4x4& in)
{
	D3DMATRIX out;
	for (int row = 0; row < 4; ++row) {
		out.m[row][0] = static_cast<float>(in[row][0]);
		out.m[row][1] = static_cast<float>(in[row][1]);
		out.m[row][2] = static_cast<float>(in[row][2]);
		out.m[row][3] = static_cast<float>(in[row][3]);
	}
	return out;
}

void Actual_SubmitDraw(
	const D3D9MeshCacheEntry* mesh,
	const Matrix4x4* modelViewMatrix,
	const Matrix3x3* normalMatrix,
	const Appearance* appearance,
	IDirect3DTexture9* texture
)
{
	D3DMATRIX worldView = ToD3DMATRIX(*modelViewMatrix);
	g_device->SetTransform(D3DTS_WORLD, &worldView);
	D3DMATRIX proj = ToD3DMATRIX(m_projection);
	g_device->SetTransform(D3DTS_PROJECTION, &proj);

	D3DMATERIAL9 mat = {};
	mat.Diffuse.r = appearance->color.r / 255.0f;
	mat.Diffuse.g = appearance->color.g / 255.0f;
	mat.Diffuse.b = appearance->color.b / 255.0f;
	mat.Diffuse.a = appearance->color.a / 255.0f;
	mat.Ambient = mat.Diffuse;

	if (appearance->shininess != 0) {
		g_device->SetRenderState(D3DRS_SPECULARENABLE, TRUE);
		mat.Specular.r = 1.0f;
		mat.Specular.g = 1.0f;
		mat.Specular.b = 1.0f;
		mat.Specular.a = 1.0f;
		mat.Power = appearance->shininess;
	}
	else {
		g_device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
		mat.Specular.r = 0.0f;
		mat.Specular.g = 0.0f;
		mat.Specular.b = 0.0f;
		mat.Specular.a = 0.0f;
		mat.Power = 0.0f;
	}

	g_device->SetMaterial(&mat);

	if (texture) {
		g_device->SetTexture(0, texture);
		g_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		g_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		g_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		g_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		g_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		g_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	}
	else {
		g_device->SetTexture(0, nullptr);
	}

	g_device->SetRenderState(D3DRS_SHADEMODE, mesh->flat ? D3DSHADE_FLAT : D3DSHADE_GOURAUD);

	g_device->SetStreamSource(0, mesh->vbo, 0, sizeof(BridgeSceneVertex));
	g_device->SetIndices(mesh->ibo);
	g_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, mesh->vertexCount, 0, mesh->indexCount / 3);
}

uint32_t Actual_FinalizeFrame(void* pixels, int pitch)
{
	g_device->EndScene();

	g_device->SetRenderTarget(0, g_oldRenderTarget);
	g_oldRenderTarget->Release();

	LPDIRECT3DSURFACE9 sysMemSurf;
	HRESULT hr =
		g_device
			->CreateOffscreenPlainSurface(m_width, m_height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &sysMemSurf, nullptr);
	if (FAILED(hr)) {
		return hr;
	}

	hr = g_device->GetRenderTargetData(g_renderTargetSurf, sysMemSurf);
	if (FAILED(hr)) {
		sysMemSurf->Release();
		return hr;
	}

	D3DLOCKED_RECT lockedRect;
	hr = sysMemSurf->LockRect(&lockedRect, nullptr, D3DLOCK_READONLY);
	if (FAILED(hr)) {
		sysMemSurf->Release();
		return hr;
	}

	BYTE* src = static_cast<BYTE*>(lockedRect.pBits);
	BYTE* dst = static_cast<BYTE*>(pixels);
	int copyWidth = m_width * 4;

	for (int y = 0; y < m_height; y++) {
		memcpy(dst + y * pitch, src + y * lockedRect.Pitch, copyWidth);
	}

	sysMemSurf->UnlockRect();
	sysMemSurf->Release();

	return hr;
}
