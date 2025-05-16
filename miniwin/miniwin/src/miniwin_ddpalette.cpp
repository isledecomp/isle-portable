#include "miniwin_ddpalette_p.h"
#include "miniwin_ddraw.h"

DirectDrawPaletteImpl::DirectDrawPaletteImpl(LPPALETTEENTRY lpColorTable)
{
}

HRESULT DirectDrawPaletteImpl::GetCaps(LPDWORD lpdwCaps)
{
	return DD_OK;
}

HRESULT DirectDrawPaletteImpl::GetEntries(DWORD dwFlags, DWORD dwBase, DWORD dwNumEntries, LPPALETTEENTRY lpEntries)
{
	return DD_OK;
}

HRESULT DirectDrawPaletteImpl::SetEntries(DWORD dwFlags, DWORD dwStartingEntry, DWORD dwCount, LPPALETTEENTRY lpEntries)
{
	return DD_OK;
}
