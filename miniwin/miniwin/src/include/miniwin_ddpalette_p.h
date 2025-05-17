#pragma once

#include "miniwin_ddraw.h"

struct DirectDrawPaletteImpl : public IDirectDrawPalette {
	DirectDrawPaletteImpl(LPPALETTEENTRY lpColorTable);
	~DirectDrawPaletteImpl() override;
	HRESULT GetCaps(LPDWORD lpdwCaps) override;
	HRESULT GetEntries(DWORD dwFlags, DWORD dwBase, DWORD dwNumEntries, LPPALETTEENTRY lpEntries) override;
	HRESULT SetEntries(DWORD dwFlags, DWORD dwStartingEntry, DWORD dwCount, LPPALETTEENTRY lpEntries) override;

private:
	SDL_Palette* m_palette;
};
