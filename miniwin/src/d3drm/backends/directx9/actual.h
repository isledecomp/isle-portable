#pragma once

#include "structs.h"

#include <SDL3/SDL.h>
#include <stdint.h>
#include <vector>

typedef float Matrix4x4[4][4];

struct BridgeVector {
	float x, y, z;
};

struct BridgeSceneLight {
	FColor color;
	BridgeVector position;
	float positional;
	BridgeVector direction;
	float directional;
};

struct BridgeSceneVertex {
	BridgeVector position;
	BridgeVector normal;
	float tu, tv;
};

struct IDirect3DRMTexture;
struct IDirect3DTexture9;
struct IDirect3DVertexBuffer9;
struct IDirect3DIndexBuffer9;
struct MeshGroup;

struct D3D9TextureCacheEntry {
	IDirect3DRMTexture* texture;
	uint32_t version;
	IDirect3DTexture9* dxTexture;
};

struct D3D9MeshCacheEntry {
	const MeshGroup* meshGroup;
	uint32_t version;
	bool flat;

	IDirect3DVertexBuffer9* vbo;
	uint32_t vertexCount;
	IDirect3DIndexBuffer9* ibo;
	uint32_t indexCount;
};

bool Actual_Initialize(void* hwnd, int width, int height);
void Actual_Shutdown();
void Actual_PushLights(const BridgeSceneLight* lightsArray, size_t count);
void Actual_SetProjection(const Matrix4x4* projection, float front, float back);
IDirect3DTexture9* UploadSurfaceToD3DTexture(SDL_Surface* surface);
void ReleaseD3DTexture(IDirect3DTexture9* dxTexture);
void ReleaseD3DVertexBuffer(IDirect3DVertexBuffer9* buffer);
void ReleaseD3DIndexBuffer(IDirect3DIndexBuffer9* buffer);
void UploadMeshBuffers(
	const void* vertices,
	int vertexSize,
	const uint16_t* indices,
	int indexSize,
	D3D9MeshCacheEntry& cache
);
uint32_t Actual_BeginFrame();
void Actual_EnableTransparency();
void Actual_SubmitDraw(
	const D3D9MeshCacheEntry* mesh,
	const Matrix4x4* modelViewMatrix,
	const Matrix3x3* normalMatrix,
	const Appearance* appearance,
	IDirect3DTexture9* texture
);
uint32_t Actual_FinalizeFrame(void* pixels, int pitch);
