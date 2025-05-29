#pragma once

#include "miniwin/d3drm.h"

#include <SDL3/SDL.h>

typedef struct PositionColorVertex {
	float x, y, z;
	float nx, ny, nz;
	Uint8 r, g, b, a;
} PositionColorVertex;

class Direct3DRMRenderer : public IDirect3DDevice2 {
public:
	virtual void SetBackbuffer(SDL_Surface* backbuffer) = 0;
	virtual void PushVertices(const PositionColorVertex* vertices, size_t count) = 0;
	virtual void SetProjection(D3DRMMATRIX4D perspective, D3DVALUE front, D3DVALUE back) = 0;
	virtual DWORD GetWidth() = 0;
	virtual DWORD GetHeight() = 0;
	virtual void GetDesc(D3DDEVICEDESC* halDesc, D3DDEVICEDESC* helDesc) = 0;
	virtual const char* GetName() = 0;
	virtual HRESULT Render() = 0;
};
