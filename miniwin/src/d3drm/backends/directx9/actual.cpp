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
static int g_width;
static int g_height;
static int g_virtualWidth;
static int g_virtualHeight;
static bool g_hasScene = false;
static std::vector<BridgeSceneLight> g_lights;
static Matrix4x4 g_projection;
static ViewportTransform g_viewportTransform;

bool Actual_Initialize(void* hwnd, int width, int height)
{
	g_hwnd = (HWND) hwnd;
	g_width = width;
	g_height = height;
	g_virtualWidth = width;
	g_virtualHeight = height;
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

	if (FAILED(hr)) {
		g_device->Release();
		g_d3d->Release();
		return false;
	}

	return true;
}

void Actual_Shutdown()
{
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
	g_lights.assign(lightsArray, lightsArray + count);
}

void Actual_SetProjection(const Matrix4x4* projection, float front, float back)
{
	memcpy(&g_projection, projection, sizeof(Matrix4x4));
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

void Actual_Resize(int width, int height, const ViewportTransform& viewportTransform)
{
	g_width = width;
	g_height = height;
	g_viewportTransform = viewportTransform;

	D3DPRESENT_PARAMETERS pp = {};
	pp.Windowed = TRUE;
	pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	pp.hDeviceWindow = g_hwnd;
	pp.BackBufferFormat = D3DFMT_A8R8G8B8;
	pp.BackBufferWidth = width;
	pp.BackBufferHeight = height;
	pp.EnableAutoDepthStencil = TRUE;
	pp.AutoDepthStencilFormat = D3DFMT_D24S8;
	g_device->Reset(&pp);
}

void StartScene()
{
	if (g_hasScene) {
		return;
	}
	g_device->BeginScene();
	g_hasScene = true;
}

void Actual_Clear(float r, float g, float b)
{
	StartScene();

	g_device->Clear(
		0,
		nullptr,
		D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
		D3DCOLOR_ARGB(255, static_cast<int>(r * 255), static_cast<int>(g * 255), static_cast<int>(b * 255)),
		1.0f,
		0
	);
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

uint32_t Actual_BeginFrame()
{
	StartScene();

	g_device->SetRenderState(D3DRS_LIGHTING, TRUE);

	g_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
	g_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	g_device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);

	g_device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	g_device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	g_device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

	for (DWORD i = 0; i < 8; ++i) {
		g_device->LightEnable(i, FALSE);
	}

	DWORD lightIdx = 0;
	for (const auto& l : g_lights) {
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
		light.Phi = M_PI;
		light.Theta = M_PI / 2;

		if (SUCCEEDED(g_device->SetLight(lightIdx, &light))) {
			g_device->LightEnable(lightIdx, TRUE);
		}
		++lightIdx;
	}

	g_device->SetFVF(D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1);

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

void SetMaterialAndTexture(const FColor& color, float shininess, IDirect3DTexture9* texture)
{
	D3DMATERIAL9 mat = {};
	mat.Diffuse.r = color.r / 255.0f;
	mat.Diffuse.g = color.g / 255.0f;
	mat.Diffuse.b = color.b / 255.0f;
	mat.Diffuse.a = color.a / 255.0f;
	mat.Ambient = mat.Diffuse;

	if (shininess != 0) {
		g_device->SetRenderState(D3DRS_SPECULARENABLE, TRUE);
		mat.Specular = {1.0f, 1.0f, 1.0f, 1.0f};
		mat.Power = shininess;
	}
	else {
		g_device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
		mat.Specular = {0.0f, 0.0f, 0.0f, 0.0f};
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
}

void Actual_SubmitDraw(
	const D3D9MeshCacheEntry* mesh,
	const Matrix4x4* modelViewMatrix,
	const Matrix4x4* worldMatrix,
	const Matrix4x4* viewMatrix,
	const Matrix3x3* normalMatrix,
	const Appearance* appearance,
	IDirect3DTexture9* texture
)
{
	D3DMATRIX proj = ToD3DMATRIX(g_projection);
	g_device->SetTransform(D3DTS_PROJECTION, &proj);
	D3DMATRIX view = ToD3DMATRIX(*viewMatrix);
	g_device->SetTransform(D3DTS_VIEW, &view);
	D3DMATRIX world = ToD3DMATRIX(*worldMatrix);
	g_device->SetTransform(D3DTS_WORLD, &world);

	SetMaterialAndTexture(
		{appearance->color.r / 255.0f,
		 appearance->color.g / 255.0f,
		 appearance->color.b / 255.0f,
		 appearance->color.a / 255.0f},
		appearance->shininess,
		texture
	);

	g_device->SetRenderState(D3DRS_SHADEMODE, mesh->flat ? D3DSHADE_FLAT : D3DSHADE_GOURAUD);

	g_device->SetStreamSource(0, mesh->vbo, 0, sizeof(BridgeSceneVertex));
	g_device->SetIndices(mesh->ibo);
	g_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, mesh->vertexCount, 0, mesh->indexCount / 3);
}

uint32_t Actual_Flip()
{
	g_device->EndScene();
	g_hasScene = false;
	return g_device->Present(nullptr, nullptr, nullptr, nullptr);
}

void Actual_Draw2DImage(IDirect3DTexture9* texture, const SDL_Rect& srcRect, const SDL_Rect& dstRect, FColor color)
{
	StartScene();

	float left = -g_viewportTransform.offsetX / g_viewportTransform.scale;
	float right = (g_width - g_viewportTransform.offsetX) / g_viewportTransform.scale;
	float top = -g_viewportTransform.offsetY / g_viewportTransform.scale;
	float bottom = (g_height - g_viewportTransform.offsetY) / g_viewportTransform.scale;

	auto virtualToScreenX = [&](float x) { return ((x - left) / (right - left)) * g_width; };
	auto virtualToScreenY = [&](float y) { return ((y - top) / (bottom - top)) * g_height; };

	float x1_virtual = static_cast<float>(dstRect.x);
	float y1_virtual = static_cast<float>(dstRect.y);
	float x2_virtual = x1_virtual + dstRect.w;
	float y2_virtual = y1_virtual + dstRect.h;

	float x1 = virtualToScreenX(x1_virtual);
	float y1 = virtualToScreenY(y1_virtual);
	float x2 = virtualToScreenX(x2_virtual);
	float y2 = virtualToScreenY(y2_virtual);

	D3DMATRIX identity =
		{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
	g_device->SetTransform(D3DTS_PROJECTION, &identity);
	g_device->SetTransform(D3DTS_VIEW, &identity);
	g_device->SetTransform(D3DTS_WORLD, &identity);

	g_device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	g_device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);

	g_device->SetRenderState(D3DRS_ZENABLE, FALSE);
	g_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	g_device->SetRenderState(D3DRS_LIGHTING, FALSE);
	g_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	g_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	g_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	SetMaterialAndTexture(color, 0, texture);

	D3DSURFACE_DESC texDesc;
	texture->GetLevelDesc(0, &texDesc);
	float texW = static_cast<float>(texDesc.Width);
	float texH = static_cast<float>(texDesc.Height);

	float u1 = static_cast<float>(srcRect.x) / texW;
	float v1 = static_cast<float>(srcRect.y) / texH;
	float u2 = static_cast<float>(srcRect.x + srcRect.w) / texW;
	float v2 = static_cast<float>(srcRect.y + srcRect.h) / texH;

	struct Vertex {
		float x, y, z, rhw;
		float u, v;
	};

	Vertex quad[4] = {
		{x1, y1, 0.0f, 1.0f, u1, v1},
		{x2, y1, 0.0f, 1.0f, u2, v1},
		{x2, y2, 0.0f, 1.0f, u2, v2},
		{x1, y2, 0.0f, 1.0f, u1, v2},
	};

	g_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
	g_device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, quad, sizeof(Vertex));
}

uint32_t Actual_Download(SDL_Surface* target)
{
	IDirect3DSurface9* backBuffer;
	HRESULT hr = g_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
	if (FAILED(hr)) {
		return hr;
	}

	IDirect3DSurface9* sysMemSurface;
	hr = g_device->CreateOffscreenPlainSurface(
		g_width,
		g_height,
		D3DFMT_A8R8G8B8,
		D3DPOOL_SYSTEMMEM,
		&sysMemSurface,
		nullptr
	);
	if (FAILED(hr)) {
		backBuffer->Release();
		return hr;
	}

	hr = g_device->GetRenderTargetData(backBuffer, sysMemSurface);
	backBuffer->Release();
	if (FAILED(hr)) {
		sysMemSurface->Release();
		return hr;
	}

	D3DLOCKED_RECT locked;
	hr = sysMemSurface->LockRect(&locked, nullptr, D3DLOCK_READONLY);
	if (FAILED(hr)) {
		sysMemSurface->Release();
		return hr;
	}

	SDL_Surface* srcSurface =
		SDL_CreateSurfaceFrom(g_width, g_height, SDL_PIXELFORMAT_ARGB8888, locked.pBits, locked.Pitch);
	if (!srcSurface) {
		sysMemSurface->UnlockRect();
		sysMemSurface->Release();
		return E_FAIL;
	}

	float srcAspect = static_cast<float>(g_width) / g_height;
	float dstAspect = static_cast<float>(target->w) / target->h;

	SDL_Rect srcRect;
	if (srcAspect > dstAspect) {
		int cropWidth = static_cast<int>(g_height * dstAspect);
		srcRect = {(g_width - cropWidth) / 2, 0, cropWidth, g_height};
	}
	else {
		int cropHeight = static_cast<int>(g_width / dstAspect);
		srcRect = {0, (g_height - cropHeight) / 2, g_width, cropHeight};
	}

	if (SDL_BlitSurfaceScaled(srcSurface, &srcRect, target, nullptr, SDL_SCALEMODE_NEAREST)) {
		SDL_DestroySurface(srcSurface);
		sysMemSurface->UnlockRect();
		sysMemSurface->Release();
		return E_FAIL;
	}

	SDL_DestroySurface(srcSurface);
	sysMemSurface->UnlockRect();
	sysMemSurface->Release();

	return D3D_OK;
}
