#include "ddpalette_impl.h"
#include "miniwin/ddraw.h"

#include <SDL3/SDL.h>

DirectDrawPaletteImpl::DirectDrawPaletteImpl(LPPALETTEENTRY lpColorTable)
{
	m_palette = SDL_CreatePalette(256);
	SetEntries(0, 0, 256, lpColorTable);
}

DirectDrawPaletteImpl::~DirectDrawPaletteImpl()
{
	SDL_DestroyPalette(m_palette);
}

HRESULT DirectDrawPaletteImpl::GetEntries(DWORD dwFlags, DWORD dwBase, DWORD dwNumEntries, LPPALETTEENTRY lpEntries)
{
	for (DWORD i = 0; i < dwNumEntries; i++) {
		lpEntries[i].peRed = m_palette->colors[dwBase + i].r;
		lpEntries[i].peGreen = m_palette->colors[dwBase + i].g;
		lpEntries[i].peBlue = m_palette->colors[dwBase + i].b;
		lpEntries[i].peFlags = PC_NONE;
	}
	return DD_OK;
}

HRESULT DirectDrawPaletteImpl::SetEntries(DWORD dwFlags, DWORD dwStartingEntry, DWORD dwCount, LPPALETTEENTRY lpEntries)
{
	SDL_Color colors[256];
	for (DWORD i = 0; i < dwCount; i++) {
		colors[i + dwStartingEntry].r = lpEntries[i].peRed;
		colors[i + dwStartingEntry].g = lpEntries[i].peGreen;
		colors[i + dwStartingEntry].b = lpEntries[i].peBlue;
		colors[i + dwStartingEntry].a = SDL_ALPHA_OPAQUE;
	}

	SDL_SetPaletteColors(m_palette, colors, dwStartingEntry, dwCount);

	return DD_OK;
}
