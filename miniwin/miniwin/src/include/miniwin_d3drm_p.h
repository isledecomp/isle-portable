#pragma once

#include "miniwin_d3drm.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <vector>

typedef struct PositionColorVertex {
	float x, y, z;
	Uint8 r, g, b, a;
} PositionColorVertex;

template <typename InterfaceType, typename ActualType, typename ArrayInterface>
class Direct3DRMArrayBase : public ArrayInterface {
public:
	~Direct3DRMArrayBase() override
	{
		for (auto* item : m_items) {
			if (item) {
				item->Release();
			}
		}
	}
	DWORD GetSize() override { return static_cast<DWORD>(m_items.size()); }
	HRESULT AddElement(InterfaceType* in) override
	{
		if (!in) {
			return DDERR_INVALIDPARAMS;
		}
		auto inImpl = static_cast<ActualType*>(in);
		if (!inImpl) {
			return DDERR_INVALIDPARAMS;
		}
		inImpl->AddRef();
		m_items.push_back(inImpl);
		return DD_OK;
	}
	HRESULT GetElement(DWORD index, InterfaceType** out) override
	{
		if (index >= m_items.size()) {
			return DDERR_INVALIDPARAMS;
		}
		*out = static_cast<InterfaceType*>(m_items[index]);
		if (*out) {
			(*out)->AddRef();
		}
		return DD_OK;
	}
	HRESULT DeleteElement(InterfaceType* element) override
	{
		auto it = std::find(m_items.begin(), m_items.end(), element);
		if (it == m_items.end()) {
			return DDERR_INVALIDPARAMS;
		}

		(*it)->Release();
		m_items.erase(it);
		return DD_OK;
	}

protected:
	std::vector<ActualType*> m_items;
};
