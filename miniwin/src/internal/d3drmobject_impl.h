#pragma once

#include "miniwin/d3drm.h"

#include <SDL3/SDL.h>
#include <vector>

template <typename T>
struct Direct3DRMObjectBaseImpl : public T {
	Direct3DRMObjectBaseImpl() : T() {}
	Direct3DRMObjectBaseImpl(const Direct3DRMObjectBaseImpl& other) : m_appData(other.m_appData), T(other)
	{
		if (other.m_name) {
			m_name = SDL_strdup(other.m_name);
		}
	}
	ULONG Release() override
	{
		if (T::m_refCount == 1) {
			for (auto it = m_callbacks.cbegin(); it != m_callbacks.cend(); it++) {
				it->first(this, it->second);
			}
			m_callbacks.clear();
		}
		SDL_free(m_name);
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
