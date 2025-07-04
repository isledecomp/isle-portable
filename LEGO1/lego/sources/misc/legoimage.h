#ifndef __LEGOIMAGE_H
#define __LEGOIMAGE_H

#include "legotypes.h"

#include <SDL3/SDL_surface.h>

class LegoStorage;

// SIZE 0x03
class LegoPaletteEntry {
public:
	LegoPaletteEntry();
	// LegoPaletteEntry(LegoU8 p_red, LegoU8 p_green, LegoU8 p_blue);
	LegoU8 GetRed() const { return m_color.r; }
	void SetRed(LegoU8 p_red) { m_color.r = p_red; }
	LegoU8 GetGreen() const { return m_color.g; }
	void SetGreen(LegoU8 p_green) { m_color.g = p_green; }
	LegoU8 GetBlue() const { return m_color.b; }
	void SetBlue(LegoU8 p_blue) { m_color.b = p_blue; }
	SDL_Color GetColor() const { return m_color; }
	void SetColor(SDL_Color p_color) { m_color = p_color; }
	LegoResult Read(LegoStorage* p_storage);
	LegoResult Write(LegoStorage* p_storage) const;

protected:
	SDL_Color m_color;
};

// 0x310
class LegoImage {
public:
	LegoImage();
	LegoImage(LegoU32 p_width, LegoU32 p_height);
	~LegoImage();
	LegoU32 GetWidth() const { return m_surface->w; }
	LegoU32 GetHeight() const { return m_surface->h; }
	LegoU32 GetCount() const { return m_palette ? m_palette->ncolors : 0; }
	SDL_Palette* GetPalette() const { return m_palette; }
	void SetPalette(SDL_Palette* p_palette)
	{
		SDL_DestroyPalette(m_palette);
		m_palette = p_palette;
	}
	void SetPaletteEntry(LegoU32 p_i, LegoPaletteEntry& p_paletteEntry)
	{
		m_palette->colors[p_i] = p_paletteEntry.GetColor();
	}
	LegoU8* GetBits() const { return (LegoU8*) m_surface->pixels; }
	LegoResult Read(LegoStorage* p_storage, LegoU32 p_square);
	LegoResult Write(LegoStorage* p_storage);

protected:
	SDL_Surface* m_surface;
	SDL_Palette* m_palette;
	//	LegoU32 m_count;                 // 0x08
	//	LegoPaletteEntry m_palette[256]; // 0x0c
	//	LegoU8* m_bits;                  // 0x30c
};

#endif // __LEGOIMAGE_H
