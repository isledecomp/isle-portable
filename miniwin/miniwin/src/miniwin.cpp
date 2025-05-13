#include "miniwin.h"

#include <SDL3/SDL.h>
#include <utility>
#include <vector>

ULONG IUnknown::AddRef()
{
	m_refCount += 1;
	return m_refCount;
}

ULONG IUnknown::Release()
{
	m_refCount -= 1;
	if (m_refCount == 0) {
		delete this;
		return 0;
	}
	return m_refCount;
}

HRESULT IUnknown::QueryInterface(const GUID& riid, void** ppvObject)
{
	return E_NOINTERFACE;
}

VOID WINAPI Sleep(DWORD dwMilliseconds)
{
	SDL_Delay(dwMilliseconds);
}
