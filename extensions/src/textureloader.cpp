#include "extensions/textureloader.h"

#include <algorithm>

using namespace Extensions;

std::map<std::string, std::string> TextureLoader::options;
std::vector<std::string> TextureLoader::excludedFiles;
bool TextureLoader::enabled = false;

void TextureLoader::Initialize()
{
	for (const auto& option : defaults) {
		if (!options.count(option.first.data())) {
			options[option.first.data()] = option.second;
		}
	}
}

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
	desc.ddpfPixelFormat.dwRGBBitCount = details->bits_per_pixel;
	desc.ddpfPixelFormat.dwRBitMask = details->Rmask;
	desc.ddpfPixelFormat.dwGBitMask = details->Gmask;
	desc.ddpfPixelFormat.dwBBitMask = details->Bmask;
	desc.ddpfPixelFormat.dwRGBAlphaBitMask = details->Amask;

	LPDIRECTDRAW pDirectDraw = VideoManager()->GetDirect3D()->DirectDraw();
	if (pDirectDraw->CreateSurface(&desc, &p_textureInfo->m_surface, NULL) != DD_OK) {
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
		SDL_Palette* sdlPalette = SDL_GetSurfacePalette(surface);
		if (!sdlPalette) {
			SDL_DestroySurface(surface);
			return false;
		}

		PALETTEENTRY entries[256];
		for (int i = 0; i < sdlPalette->ncolors; ++i) {
			entries[i].peRed = sdlPalette->colors[i].r;
			entries[i].peGreen = sdlPalette->colors[i].g;
			entries[i].peBlue = sdlPalette->colors[i].b;
			entries[i].peFlags = PC_NONE;
		}

		LPDIRECTDRAWPALETTE ddPalette = nullptr;
		if (pDirectDraw->CreatePalette(DDPCAPS_8BIT | DDPCAPS_ALLOW256, entries, &ddPalette, NULL) != DD_OK) {
			SDL_DestroySurface(surface);
			return false;
		}

		p_textureInfo->m_surface->SetPalette(ddPalette);
		ddPalette->Release();
	}

	memcpy(dst, srcPixels, surface->pitch * surface->h);
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
	if (std::find(excludedFiles.begin(), excludedFiles.end(), p_name) != excludedFiles.end()) {
		return nullptr;
	}

	SDL_Surface* surface;
	const char* texturePath = options["texture loader:texture path"].c_str();
	MxString path = MxString(MxOmni::GetHD()) + texturePath + "/" + p_name + ".bmp";

	path.MapPathToFilesystem();
	if (!(surface = SDL_LoadBMP(path.GetData()))) {
		path = MxString(MxOmni::GetCD()) + texturePath + "/" + p_name + ".bmp";
		path.MapPathToFilesystem();
		surface = SDL_LoadBMP(path.GetData());
	}

	return surface;
}
