#include "legoimage.h"

#include "decomp.h"
#include "legostorage.h"
#include "memory.h"

DECOMP_SIZE_ASSERT(LegoPaletteEntry, 0x03);
DECOMP_SIZE_ASSERT(LegoImage, 0x310);

// FUNCTION: LEGO1 0x100994c0
LegoPaletteEntry::LegoPaletteEntry()
{
	m_color.r = 0;
	m_color.g = 0;
	m_color.b = 0;
	m_color.a = SDL_ALPHA_OPAQUE;
}

// FUNCTION: LEGO1 0x100994d0
LegoResult LegoPaletteEntry::Read(LegoStorage* p_storage)
{
	LegoResult result;
	if ((result = p_storage->Read(&m_color.r, sizeof(Uint8))) != SUCCESS) {
		return result;
	}
	if ((result = p_storage->Read(&m_color.g, sizeof(Uint8))) != SUCCESS) {
		return result;
	}
	if ((result = p_storage->Read(&m_color.b, sizeof(Uint8))) != SUCCESS) {
		return result;
	}
	return SUCCESS;
}

// FUNCTION: LEGO1 0x10099520
LegoResult LegoPaletteEntry::Write(LegoStorage* p_storage) const
{
	LegoResult result;
	if ((result = p_storage->Write(&m_color.r, sizeof(Uint8))) != SUCCESS) {
		return result;
	}
	if ((result = p_storage->Write(&m_color.g, sizeof(Uint8))) != SUCCESS) {
		return result;
	}
	if ((result = p_storage->Write(&m_color.b, sizeof(Uint8))) != SUCCESS) {
		return result;
	}
	return SUCCESS;
}

// FUNCTION: LEGO1 0x10099570
LegoImage::LegoImage()
{
	m_surface = NULL;
	m_palette = NULL;
}

// FUNCTION: LEGO1 0x100995a0
LegoImage::LegoImage(LegoU32 p_width, LegoU32 p_height)
{
	m_surface = SDL_CreateSurface(p_width, p_height, SDL_PIXELFORMAT_INDEX8);
	m_palette = NULL;
}

// FUNCTION: LEGO1 0x100995f0
LegoImage::~LegoImage()
{
	SDL_DestroySurface(m_surface);
	SDL_DestroyPalette(m_palette);
}

// FUNCTION: LEGO1 0x10099610
LegoResult LegoImage::Read(LegoStorage* p_storage, LegoU32 p_square)
{
	LegoResult result;
	LegoU32 width, height, count;
	if ((result = p_storage->Read(&width, sizeof(LegoU32))) != SUCCESS) {
		return result;
	}
	if ((result = p_storage->Read(&height, sizeof(LegoU32))) != SUCCESS) {
		return result;
	}
	if ((result = p_storage->Read(&count, sizeof(LegoU32))) != SUCCESS) {
		return result;
	}
	if (m_palette) {
		SDL_DestroyPalette(m_palette);
	}
	m_palette = SDL_CreatePalette(count);
	for (LegoU32 i = 0; i < count; i++) {
		LegoPaletteEntry paletteEntry;
		if ((result = paletteEntry.Read(p_storage)) != SUCCESS) {
			return result;
		}
		m_palette->colors[i] = paletteEntry.GetColor();
	}
	if (m_surface) {
		SDL_DestroySurface(m_surface);
	}
	m_surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_INDEX8);
	if ((result = p_storage->Read(m_surface->pixels, width * height)) != SUCCESS) {
		return result;
	}

	if (p_square && width != height) {
		SDL_Surface* newSurface;

		if (height < width) {
			LegoU32 aspect = width / height;
			newSurface = SDL_CreateSurface(width, width, SDL_PIXELFORMAT_INDEX8);
			LegoU8* src = (LegoU8*) m_surface->pixels;
			LegoU8* dst = (LegoU8*) newSurface->pixels;

			for (LegoU32 row = 0; row < height; row++) {
				if (aspect) {
					for (LegoU32 dup = aspect; dup; dup--) {
						memcpy(dst, src, width);
						dst += width;
					}
				}
				src += width;
			}

			height = width;
		}
		else {
			LegoU32 aspect = height / width;
			newSurface = SDL_CreateSurface(height, height, SDL_PIXELFORMAT_INDEX8);
			LegoU8* src = (LegoU8*) m_surface->pixels;
			LegoU8* dst = (LegoU8*) newSurface->pixels;

			for (LegoU32 row = 0; row < height; row++) {
				for (LegoU32 col = 0; col < width; col++) {
					if (aspect) {
						for (LegoU32 dup = aspect; dup; dup--) {
							*dst = *src;
							dst++;
						}
					}

					src++;
				}
			}

			width = height;
		}

		SDL_DestroySurface(m_surface);
		m_surface = newSurface;
	}

	return SUCCESS;
}

// FUNCTION: LEGO1 0x100997e0
LegoResult LegoImage::Write(LegoStorage* p_storage)
{
	LegoResult result;
	if ((result = p_storage->Write(&m_surface->w, sizeof(int))) != SUCCESS) {
		return result;
	}
	if ((result = p_storage->Write(&m_surface->h, sizeof(int))) != SUCCESS) {
		return result;
	}
	if ((result = p_storage->Write(&m_surface->h, sizeof(int))) != SUCCESS) {
		return result;
	}
	if (m_palette) {
		LegoPaletteEntry paletteEntry;
		for (LegoU32 i = 0; i < m_palette->ncolors; i++) {
			paletteEntry.SetColor(m_palette->colors[i]);
			if ((result = paletteEntry.Write(p_storage)) != SUCCESS) {
				return result;
			}
		}
		if ((result = p_storage->Write(m_surface->pixels, m_surface->w * m_surface->h)) != SUCCESS) {
			return result;
		}
	}
	return SUCCESS;
}
