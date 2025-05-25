#pragma once

#include "miniwin_ddraw.h"

struct DirectDrawPalette_SDL3GPUImpl : public IDirectDrawPalette {
	DirectDrawPalette_SDL3GPUImpl(LPPALETTEENTRY lpColorTable);
	~DirectDrawPalette_SDL3GPUImpl() override;
	HRESULT GetEntries(DWORD dwFlags, DWORD dwBase, DWORD dwNumEntries, LPPALETTEENTRY lpEntries) override;
	HRESULT SetEntries(DWORD dwFlags, DWORD dwStartingEntry, DWORD dwCount, LPPALETTEENTRY lpEntries) override;

	SDL_Palette* m_palette;
};
