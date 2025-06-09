#pragma once

#include "mathutils.h"
#include "miniwin/d3drm.h"
#include "miniwin/miniwindevice.h"

#include <SDL3/SDL.h>

#define NO_TEXTURE_ID 0xffffffff

static_assert(sizeof(D3DRMVERTEX) == 32);

struct Appearance {
	SDL_Color color;
	float shininess;
	Uint32 textureId;
	Uint32 flat;
};

struct FColor {
	float r, g, b, a;
};

struct SceneLight {
	FColor color;
	D3DVECTOR position;
	float positional = 0.f; // position is valid if 1.f
	D3DVECTOR direction;
	float directional = 0.f; // direction is valid if 1.f
};
static_assert(sizeof(SceneLight) == 48);

class Direct3DRMRenderer : public IDirect3DDevice2 {
public:
	virtual void PushLights(const SceneLight* vertices, size_t count) = 0;
	virtual void SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back) = 0;
	virtual Uint32 GetTextureId(IDirect3DRMTexture* texture) = 0;
	virtual DWORD GetWidth() = 0;
	virtual DWORD GetHeight() = 0;
	virtual void GetDesc(D3DDEVICEDESC* halDesc, D3DDEVICEDESC* helDesc) = 0;
	virtual const char* GetName() = 0;
	virtual HRESULT BeginFrame(const D3DRMMATRIX4D& viewMatrix) = 0;
	virtual void SubmitDraw(
		const D3DRMVERTEX* vertices,
		const size_t vertexCount,
		const DWORD* indices,
		const size_t indexCount,
		const D3DRMMATRIX4D& worldMatrix,
		const Matrix3x3& normalMatrix,
		const Appearance& appearance
	) = 0;
	virtual HRESULT FinalizeFrame() = 0;

	float GetShininessFactor() { return m_shininessFactor; }
	HRESULT SetShininessFactor(float factor)
	{
		if (factor < 0.f) {
			return DDERR_GENERIC;
		}
		m_shininessFactor = factor;
		return DD_OK;
	}
	virtual HRESULT GetDescription(char* buffer, int bufferSize) = 0;

protected:
	float m_shininessFactor = 1.f;
};
