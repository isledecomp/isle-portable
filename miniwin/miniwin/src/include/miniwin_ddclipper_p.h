#pragma once

#include <SDL3/SDL.h>
#include <miniwin_ddraw.h>

class DirectDrawImpl;

struct DirectDrawClipperImpl : public IDirectDrawClipper {
	DirectDrawClipperImpl(DirectDrawImpl* lpDD);
	~DirectDrawClipperImpl() override;

	// IDirectDrawClipper interface
	HRESULT SetHWnd(DWORD unnamedParam1, HWND hWnd) override;
};
