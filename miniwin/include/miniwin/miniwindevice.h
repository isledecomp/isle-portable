#pragma once

#include <SDL3/SDL_events.h>

DEFINE_GUID(IID_IDirect3DRMMiniwinDevice, 0x6eb09673, 0x8d30, 0x4d8a, 0x8d, 0x81, 0x34, 0xea, 0x69, 0x30, 0x12, 0x01);

struct IDirect3DRMMiniwinDevice : virtual public IUnknown {
	virtual bool ConvertEventToRenderCoordinates(SDL_Event* event) = 0;
	virtual bool ConvertRenderToWindowCoordinates(Sint32 inX, Sint32 inY, Sint32& outX, Sint32& outY) = 0;
};
