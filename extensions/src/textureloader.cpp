#include "extensions/textureloader.h"

using namespace Extensions;

bool TextureLoader::PatchTexture(LegoTextureInfo* p_textureInfo)
{
	SDL_Surface* surface = FindTexture(p_textureInfo->m_name);
	if (!surface) {
		return false;
	}

	const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(surface->format);

	DDSURFACEDESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.dwSize = sizeof(desc);
	desc.dwFlags = DDSD_PIXELFORMAT | DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS;
	desc.ddsCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_SYSTEMMEMORY;
	desc.ddpfPixelFormat.dwSize = sizeof(desc.ddpfPixelFormat);
	desc.dwWidth = surface->w;
	desc.dwHeight = surface->h;
	desc.ddpfPixelFormat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
	desc.ddpfPixelFormat.dwRGBBitCount = details->bits_per_pixel == 8 ? 24 : details->bits_per_pixel;
	desc.ddpfPixelFormat.dwRBitMask = details->Rmask;
	desc.ddpfPixelFormat.dwGBitMask = details->Gmask;
	desc.ddpfPixelFormat.dwBBitMask = details->Bmask;
	desc.ddpfPixelFormat.dwRGBAlphaBitMask = details->Amask;

	if (VideoManager()->GetDirect3D()->DirectDraw()->CreateSurface(&desc, &p_textureInfo->m_surface, NULL) != DD_OK) {
		SDL_DestroySurface(surface);
		return false;
	}

	memset(&desc, 0, sizeof(desc));
	desc.dwSize = sizeof(desc);

	if (p_textureInfo->m_surface->Lock(NULL, &desc, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WRITEONLY, NULL) != DD_OK) {
		SDL_DestroySurface(surface);
		return false;
	}

	MxU8* dst = (MxU8*) desc.lpSurface;
	Uint8* srcPixels = (Uint8*) surface->pixels;

	if (details->bits_per_pixel == 8) {
		SDL_Palette* palette = SDL_GetSurfacePalette(surface);
		if (palette) {
			for (int y = 0; y < surface->h; ++y) {
				Uint8* srcRow = srcPixels + y * surface->pitch;
				Uint8* dstRow = dst + y * desc.lPitch;
				for (int x = 0; x < surface->w; ++x) {
					SDL_Color color = palette->colors[srcRow[x]];
					dstRow[x * 3 + 0] = color.r;
					dstRow[x * 3 + 1] = color.g;
					dstRow[x * 3 + 2] = color.b;
				}
			}
		}
		else {
			p_textureInfo->m_surface->Unlock(desc.lpSurface);
			SDL_DestroySurface(surface);
			return false;
		}
	}
	else {
		memcpy(dst, srcPixels, surface->pitch * surface->h);
	}

	p_textureInfo->m_surface->Unlock(desc.lpSurface);
	p_textureInfo->m_palette = NULL;

	if (((TglImpl::RendererImpl*) VideoManager()->GetRenderer())
			->CreateTextureFromSurface(p_textureInfo->m_surface, &p_textureInfo->m_texture) != D3DRM_OK) {
		SDL_DestroySurface(surface);
		return false;
	}

	p_textureInfo->m_texture->SetAppData((LPD3DRM_APPDATA) p_textureInfo);
	SDL_DestroySurface(surface);
	return true;
}

SDL_Surface* TextureLoader::FindTexture(const char* p_name)
{
	SDL_Surface* surface;
	MxString path = MxString(MxOmni::GetHD()) + texturePath + p_name + ".bmp";

	path.MapPathToFilesystem();
	if (!(surface = SDL_LoadBMP(path.GetData()))) {
		path = MxString(MxOmni::GetCD()) + texturePath + p_name + ".bmp";
		path.MapPathToFilesystem();
		surface = SDL_LoadBMP(path.GetData());
	}

	return surface;
}
