#pragma once

#include "miniwin_d3drm.h"

#include <SDL3/SDL.h>
#include <algorithm>
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
