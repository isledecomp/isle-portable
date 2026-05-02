#include "d3drmrenderer.h"
#include "d3drmrenderer_glide.h"
#include "ddsurface_impl.h"
#include "mathutils.h"
#include "meshutils.h"
#include "miniwin.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>

extern "C"
{
#include <glide.h>
}

static void ProjectVertex(
	const D3DRMMATRIX4D& projection,
	int screenW,
	int screenH,
	const D3DVECTOR& v,
	float& outX,
	float& outY,
	float& outZ,
	float& outW
)
{
	float px = projection[0][0] * v.x + projection[1][0] * v.y + projection[2][0] * v.z + projection[3][0];
	float py = projection[0][1] * v.x + projection[1][1] * v.y + projection[2][1] * v.z + projection[3][1];
	float pz = projection[0][2] * v.x + projection[1][2] * v.y + projection[2][2] * v.z + projection[3][2];
	float pw = projection[0][3] * v.x + projection[1][3] * v.y + projection[2][3] * v.z + projection[3][3];

	outW = pw;
	if (pw != 0.0f) {
		float invW = 1.0f / pw;
		px *= invW;
		py *= invW;
		pz *= invW;
	}

	outX = (px * 0.5f + 0.5f) * screenW;
	outY = (1.0f - (py * 0.5f + 0.5f)) * screenH;
	outZ = pz;
}

static SDL_Color ApplyLighting(
	const std::vector<SceneLight>& lights,
	const D3DVECTOR& position,
	const D3DVECTOR& oNormal,
	const Matrix3x3& normalMatrix,
	const Appearance& appearance
)
{
	FColor specular = {0, 0, 0, 0};
	FColor diffuse = {0, 0, 0, 0};

	D3DVECTOR normal = Normalize(TransformNormal(oNormal, normalMatrix));

	for (const auto& light : lights) {
		FColor lightColor = light.color;

		if (light.positional == 0.0f && light.directional == 0.0f) {
			diffuse.r += lightColor.r;
			diffuse.g += lightColor.g;
			diffuse.b += lightColor.b;
			continue;
		}

		D3DVECTOR lightVec;
		if (light.directional == 1.0f) {
			lightVec = {-light.direction.x, -light.direction.y, -light.direction.z};
		}
		else if (light.positional == 1.0f) {
			lightVec = {light.position.x - position.x, light.position.y - position.y, light.position.z - position.z};
		}
		lightVec = Normalize(lightVec);

		float dotNL = DotProduct(normal, lightVec);
		if (dotNL > 0.0f) {
			diffuse.r += dotNL * lightColor.r;
			diffuse.g += dotNL * lightColor.g;
			diffuse.b += dotNL * lightColor.b;

			if (appearance.shininess > 0.0f && light.directional == 1.0f) {
				D3DVECTOR viewVec = Normalize({-position.x, -position.y, -position.z});
				D3DVECTOR H = Normalize({lightVec.x + viewVec.x, lightVec.y + viewVec.y, lightVec.z + viewVec.z});

				float dotNH = std::max(DotProduct(normal, H), 0.0f);
				float spec = std::pow(dotNH, appearance.shininess);

				specular.r += spec * lightColor.r;
				specular.g += spec * lightColor.g;
				specular.b += spec * lightColor.b;
			}
		}
	}

	return SDL_Color{
		static_cast<Uint8>(std::min(255.0f, diffuse.r * appearance.color.r + specular.r * 255.0f)),
		static_cast<Uint8>(std::min(255.0f, diffuse.g * appearance.color.g + specular.g * 255.0f)),
		static_cast<Uint8>(std::min(255.0f, diffuse.b * appearance.color.b + specular.b * 255.0f)),
		appearance.color.a
	};
}

// Clip a vertex against a general plane, interpolating all attributes
static D3DRMVERTEX ClipEdgePlane(const D3DRMVERTEX& a, const D3DRMVERTEX& b, const Plane& plane)
{
	float da = DotProduct(plane.normal, a.position) + plane.d;
	float db = DotProduct(plane.normal, b.position) + plane.d;
	float t = da / (da - db);

	D3DRMVERTEX result;
	result.position.x = a.position.x + t * (b.position.x - a.position.x);
	result.position.y = a.position.y + t * (b.position.y - a.position.y);
	result.position.z = a.position.z + t * (b.position.z - a.position.z);
	result.normal.x = a.normal.x + t * (b.normal.x - a.normal.x);
	result.normal.y = a.normal.y + t * (b.normal.y - a.normal.y);
	result.normal.z = a.normal.z + t * (b.normal.z - a.normal.z);
	result.texCoord.u = a.texCoord.u + t * (b.texCoord.u - a.texCoord.u);
	result.texCoord.v = a.texCoord.v + t * (b.texCoord.v - a.texCoord.v);
	return result;
}

// Sutherland-Hodgman clip polygon against one plane
static int ClipPolygonAgainstPlane(const D3DRMVERTEX* in, int inCount, D3DRMVERTEX* out, const Plane& plane)
{
	if (inCount < 3) {
		return 0;
	}
	int outCount = 0;
	for (int i = 0; i < inCount; ++i) {
		const D3DRMVERTEX& cur = in[i];
		const D3DRMVERTEX& next = in[(i + 1) % inCount];
		float dCur = DotProduct(plane.normal, cur.position) + plane.d;
		float dNext = DotProduct(plane.normal, next.position) + plane.d;
		if (dCur >= 0) {
			out[outCount++] = cur;
			if (dNext < 0) {
				out[outCount++] = ClipEdgePlane(cur, next, plane);
			}
		}
		else if (dNext >= 0) {
			out[outCount++] = ClipEdgePlane(cur, next, plane);
		}
	}
	return outCount;
}

static bool IsTriangleOutsideViewCone(
	const D3DVECTOR& v0,
	const D3DVECTOR& v1,
	const D3DVECTOR& v2,
	const Plane* frustumPlanes
)
{
	for (int i = 0; i < 4; ++i) {
		const Plane& plane = frustumPlanes[i];
		float d0 = DotProduct(plane.normal, v0) + plane.d;
		float d1 = DotProduct(plane.normal, v1) + plane.d;
		float d2 = DotProduct(plane.normal, v2) + plane.d;
		if (d0 < 0 && d1 < 0 && d2 < 0) {
			return true;
		}
	}
	return false;
}

static GlideMeshEntry UploadMeshGlide(const MeshGroup& meshGroup)
{
	GlideMeshEntry cache;
	cache.meshGroup = &meshGroup;
	cache.version = meshGroup.version;
	cache.flat = meshGroup.quality == D3DRMRENDER_FLAT || meshGroup.quality == D3DRMRENDER_UNLITFLAT;

	if (cache.flat) {
		FlattenSurfaces(
			meshGroup.vertices.data(),
			meshGroup.vertices.size(),
			meshGroup.indices.data(),
			meshGroup.indices.size(),
			meshGroup.texture != nullptr,
			cache.flatVertices,
			cache.flatIndices
		);
	}

	return cache;
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

Direct3DRMGlideRenderer::Direct3DRMGlideRenderer(int width, int height)
	: m_transparencyEnabled(false), m_nextTextureAddress(0)
{
	m_virtualWidth = width;
	m_virtualHeight = height;
	m_width = 640;
	m_height = 480;

	memset(m_projection, 0, sizeof(m_projection));
	m_frontClip = 0.1f;
	m_backClip = 1000.0f;

	grGlideInit();
	grSstSelect(0);

#ifdef GLIDE3
	GrContext_t ctx = grSstWinOpen(
		0,
		GR_RESOLUTION_640x480,
		GR_REFRESH_60Hz,
		GR_COLORFORMAT_ABGR,
		GR_ORIGIN_UPPER_LEFT,
		2, // double buffer
		1  // aux buffer (z-buffer)
	);
	if (!ctx) {
		SDL_Log("Glide: grSstWinOpen failed");
		return;
	}
	m_glideContext = ctx;
#else
	if (!grSstWinOpen(
			0,
			GR_RESOLUTION_640x480,
			GR_REFRESH_60Hz,
			GR_COLORFORMAT_ABGR,
			GR_ORIGIN_UPPER_LEFT,
			2, // double buffer
			1  // aux buffer (z-buffer)
		)) {
		SDL_Log("Glide: grSstWinOpen failed");
		return;
	}
#endif

#ifdef GLIDE3
	// Set up vertex layout for Glide 3
	grVertexLayout(GR_PARAM_XY, offsetof(GlideVertex, x), GR_PARAM_ENABLE);
	grVertexLayout(GR_PARAM_Z, offsetof(GlideVertex, ooz), GR_PARAM_ENABLE);
	grVertexLayout(GR_PARAM_Q, offsetof(GlideVertex, oow), GR_PARAM_ENABLE);
	grVertexLayout(GR_PARAM_RGB, offsetof(GlideVertex, r), GR_PARAM_ENABLE);
	grVertexLayout(GR_PARAM_A, offsetof(GlideVertex, a), GR_PARAM_ENABLE);
	grVertexLayout(GR_PARAM_ST0, offsetof(GlideVertex, sow), GR_PARAM_ENABLE);
#endif

	// Enable W-buffer (uses oow field directly for depth)
	grDepthBufferMode(GR_DEPTHBUFFER_WBUFFER);
	grDepthBufferFunction(GR_CMP_LESS);
	grDepthMask(FXTRUE);

	// Enable backface culling in software (matching software renderer)
	// Don't use grCullMode as winding may differ after our projection
	grCullMode(GR_CULL_DISABLE);

	// Default color combine: vertex color only (untextured)
	grColorCombine(
		GR_COMBINE_FUNCTION_LOCAL,
		GR_COMBINE_FACTOR_NONE,
		GR_COMBINE_LOCAL_ITERATED,
		GR_COMBINE_OTHER_NONE,
		FXFALSE
	);

	// Default dithering
	grDitherMode(GR_DITHER_4x4);

	// Default alpha blend (opaque)
	grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO, GR_BLEND_ZERO);

	// Initialize texture memory allocator
	m_nextTextureAddress = grTexMinAddress(GR_TMU0);

	ViewportTransform viewportTransform = {1.0f, 0.0f, 0.0f};
	Resize(width, height, viewportTransform);
}

Direct3DRMGlideRenderer::~Direct3DRMGlideRenderer()
{
#ifdef GLIDE3
	grSstWinClose(m_glideContext);
#else
	grSstWinClose();
#endif
	grGlideShutdown();
}

HRESULT Direct3DRMGlideRenderer::BeginFrame()
{
	return S_OK;
}

HRESULT Direct3DRMGlideRenderer::FinalizeFrame()
{
	// Reset alpha blend and depth mask if transparency was used this frame
	if (m_transparencyEnabled) {
		m_transparencyEnabled = false;
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO, GR_BLEND_ZERO);
		grDepthMask(FXTRUE);
	}
	return S_OK;
}

void Direct3DRMGlideRenderer::Clear(float r, float g, float b)
{
	// GR_COLORFORMAT_ABGR: color is packed as 0xAABBGGRR
	GrColor_t color = 0xFF000000 | ((Uint32) (b * 255.0f) << 16) | ((Uint32) (g * 255.0f) << 8) | (Uint32) (r * 255.0f);
	grBufferClear(color, 0, 0xFFFF);
}

void Direct3DRMGlideRenderer::Flip()
{
	grBufferSwap(0);
}

void Direct3DRMGlideRenderer::Resize(int width, int height, const ViewportTransform& viewportTransform)
{
	// Voodoo is fixed at 640x480, so we just store the viewport transform
	m_viewportTransform = viewportTransform;
	m_width = 640;
	m_height = 480;

	m_viewportTransform.scale =
		std::min(static_cast<float>(m_width) / m_virtualWidth, static_cast<float>(m_height) / m_virtualHeight);
	m_viewportTransform.offsetX = (m_width - (m_virtualWidth * m_viewportTransform.scale)) / 2.0f;
	m_viewportTransform.offsetY = (m_height - (m_virtualHeight * m_viewportTransform.scale)) / 2.0f;
}

void Direct3DRMGlideRenderer::PushLights(const SceneLight* lights, size_t count)
{
	m_lights.assign(lights, lights + count);
}

void Direct3DRMGlideRenderer::SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back)
{
	m_frontClip = front;
	m_backClip = back;
	memcpy(m_projection, projection, sizeof(D3DRMMATRIX4D));
}

void Direct3DRMGlideRenderer::SetFrustumPlanes(const Plane* frustumPlanes)
{
	memcpy(m_frustumPlanes, frustumPlanes, sizeof(m_frustumPlanes));
}

void Direct3DRMGlideRenderer::EnableTransparency()
{
	m_transparencyEnabled = true;
	grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ZERO, GR_BLEND_ZERO);
	grDepthMask(FXFALSE); // don't write to depth buffer for transparent objects
}

void Direct3DRMGlideRenderer::SetDither(bool dither)
{
	grDitherMode(dither ? GR_DITHER_4x4 : GR_DITHER_DISABLE);
}

void Direct3DRMGlideRenderer::SetPalette(SDL_Palette* palette)
{
	m_palette = palette;
	if (palette && palette->ncolors >= 256) {
		// Upload palette to Glide texture palette table
		GuTexPalette glidePal;
		for (int i = 0; i < 256; ++i) {
			// Texture palette is always ARGB regardless of GR_COLORFORMAT
			glidePal.data[i] =
				(static_cast<FxU32>(palette->colors[i].a) << 24) | (static_cast<FxU32>(palette->colors[i].r) << 16) |
				(static_cast<FxU32>(palette->colors[i].g) << 8) | (static_cast<FxU32>(palette->colors[i].b));
		}
#ifdef GLIDE3
		grTexDownloadTable(GR_TEXTABLE_PALETTE, &glidePal);
#else
		grTexDownloadTable(GR_TMU0, GR_TEXTABLE_PALETTE, &glidePal);
#endif
		m_paletteUploaded = true;
	}
}

static GrLOD_t GlideLODFromSize(int size)
{
	switch (size) {
	case 256:
		return GR_LOD_LOG2_256;
	case 128:
		return GR_LOD_LOG2_128;
	case 64:
		return GR_LOD_LOG2_64;
	case 32:
		return GR_LOD_LOG2_32;
	case 16:
		return GR_LOD_LOG2_16;
	case 8:
		return GR_LOD_LOG2_8;
	case 4:
		return GR_LOD_LOG2_4;
	case 2:
		return GR_LOD_LOG2_2;
	case 1:
		return GR_LOD_LOG2_1;
	default:
		return GR_LOD_LOG2_256;
	}
}

static int NextPow2(int v)
{
	if (v <= 1) {
		return 1;
	}
	int p = 1;
	while (p < v) {
		p <<= 1;
	}
	if (p > 256) {
		p = 256; // Glide max
	}
	return p;
}

static void UploadGlideTexture(GlideTextureEntry& entry, SDL_Surface* surface, FxU32& nextAddress)
{
	int srcW = surface->w;
	int srcH = surface->h;
	int texW = NextPow2(srcW);
	int texH = NextPow2(srcH);

	// Glide requires square aspect ratio or specific aspect ratios.
	// Use the larger dimension for both LOD values.
	int maxDim = texW > texH ? texW : texH;
	int minDim = texW < texH ? texW : texH;

	GrLOD_t largeLod = GlideLODFromSize(maxDim);
	GrLOD_t smallLod = largeLod; // single mip level

	GrAspectRatio_t aspect;
	int ratio = maxDim / minDim;
	if (texW == texH) {
		aspect = GR_ASPECT_LOG2_1x1;
	}
	else if (texW > texH) {
		switch (ratio) {
		case 2:
			aspect = GR_ASPECT_LOG2_2x1;
			break;
		case 4:
			aspect = GR_ASPECT_LOG2_4x1;
			break;
		case 8:
			aspect = GR_ASPECT_LOG2_8x1;
			break;
		default:
			aspect = GR_ASPECT_LOG2_8x1;
			break;
		}
	}
	else {
		switch (ratio) {
		case 2:
			aspect = GR_ASPECT_LOG2_1x2;
			break;
		case 4:
			aspect = GR_ASPECT_LOG2_1x4;
			break;
		case 8:
			aspect = GR_ASPECT_LOG2_1x8;
			break;
		default:
			aspect = GR_ASPECT_LOG2_1x8;
			break;
		}
	}

	entry.info.smallLodLog2 = smallLod;
	entry.info.largeLodLog2 = largeLod;
	entry.info.aspectRatioLog2 = aspect;
	entry.info.format = GR_TEXFMT_P_8;

	// Build 8-bit texture data, scaling if needed
	std::vector<Uint8> texData(texW * texH);

	SDL_LockSurface(surface);
	Uint8* srcPixels = static_cast<Uint8*>(surface->pixels);
	int srcPitch = surface->pitch;

	for (int y = 0; y < texH; ++y) {
		int srcY = (y * srcH) / texH;
		if (srcY >= srcH) {
			srcY = srcH - 1;
		}
		for (int x = 0; x < texW; ++x) {
			int srcX = (x * srcW) / texW;
			if (srcX >= srcW) {
				srcX = srcW - 1;
			}
			texData[y * texW + x] = srcPixels[srcY * srcPitch + srcX];
		}
	}
	SDL_UnlockSurface(surface);

	entry.info.data = texData.data();

	// Calculate memory needed and allocate
	FxU32 memNeeded = grTexCalcMemRequired(smallLod, largeLod, aspect, GR_TEXFMT_P_8);

	// Textures cannot span a 2MB boundary
	const FxU32 BOUNDARY = 2 * 1024 * 1024;
	FxU32 boundaryStart = (nextAddress / BOUNDARY) * BOUNDARY;
	FxU32 boundaryEnd = boundaryStart + BOUNDARY;
	if (nextAddress + memNeeded > boundaryEnd) {
		// Skip to next 2MB boundary
		nextAddress = boundaryEnd;
	}

	// Check if we have space
	FxU32 texMemEnd = grTexMaxAddress(GR_TMU0);
	if (nextAddress + memNeeded > texMemEnd) {
		// Out of texture memory
		entry.startAddress = 0xFFFFFFFF;
		entry.texW = 0;
		entry.texH = 0;
		return;
	}

	entry.startAddress = nextAddress;
	entry.texW = 256; // Glide 3 tex coords are always in [0, 256] range
	entry.texH = 256;

	grTexDownloadMipMap(GR_TMU0, nextAddress, GR_MIPMAPLEVELMASK_BOTH, &entry.info);

	nextAddress += memNeeded;
	entry.info.data = nullptr; // don't keep dangling pointer
}

// Convert any surface to 8-bit indexed using the game palette
static SDL_Surface* ConvertToIndexed(SDL_Surface* surface, SDL_Palette* palette)
{
	int w = surface->w;
	int h = surface->h;
	int bpp = SDL_GetPixelFormatDetails(surface->format)->bytes_per_pixel;

	SDL_Surface* indexed = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_INDEX8);
	SDL_SetSurfacePalette(indexed, palette);
	SDL_LockSurface(indexed);

	Uint8* dst = static_cast<Uint8*>(indexed->pixels);
	int dstPitch = indexed->pitch;

	if (bpp == 1) {
		// 8-bit source: remap palette indices
		SDL_LockSurface(surface);
		SDL_Palette* srcPal = SDL_GetSurfacePalette(surface);
		Uint8* src = static_cast<Uint8*>(surface->pixels);
		int srcPitch = surface->pitch;

		// Build remap table
		Uint8 remap[256];
		if (srcPal && srcPal != palette) {
			for (int i = 0; i < 256; ++i) {
				if (i >= srcPal->ncolors) {
					remap[i] = 0;
					continue;
				}
				int sr = srcPal->colors[i].r;
				int sg = srcPal->colors[i].g;
				int sb = srcPal->colors[i].b;
				int bestDist = INT_MAX;
				Uint8 bestIdx = 0;
				for (int c = 0; c < palette->ncolors; ++c) {
					int dr = palette->colors[c].r - sr;
					int dg = palette->colors[c].g - sg;
					int db = palette->colors[c].b - sb;
					int dist = dr * dr + dg * dg + db * db;
					if (dist < bestDist) {
						bestDist = dist;
						bestIdx = static_cast<Uint8>(c);
						if (dist == 0) {
							break;
						}
					}
				}
				remap[i] = bestIdx;
			}
		}
		else {
			for (int i = 0; i < 256; ++i) {
				remap[i] = static_cast<Uint8>(i);
			}
		}

		for (int y = 0; y < h; ++y) {
			Uint8* srcRow = src + y * srcPitch;
			Uint8* dstRow = dst + y * dstPitch;
			for (int x = 0; x < w; ++x) {
				dstRow[x] = remap[srcRow[x]];
			}
		}
		SDL_UnlockSurface(surface);
	}
	else {
		// Non-paletted source: convert to RGBA32 first for consistent byte order
		SDL_Surface* rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
		SDL_LockSurface(rgba);

		Uint8* src = static_cast<Uint8*>(rgba->pixels);
		int srcPitch = rgba->pitch;

		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < w; ++x) {
				Uint8* px = src + y * srcPitch + x * 4;
				int pr = px[0], pg = px[1], pb = px[2], pa = px[3];

				if (pa == 0) {
					dst[y * dstPitch + x] = 0;
				}
				else {
					int bestDist = INT_MAX;
					Uint8 bestIdx = 0;
					for (int c = 1; c < palette->ncolors; ++c) {
						int dr = pr - palette->colors[c].r;
						int dg = pg - palette->colors[c].g;
						int db = pb - palette->colors[c].b;
						int dist = dr * dr + dg * dg + db * db;
						if (dist < bestDist) {
							bestDist = dist;
							bestIdx = static_cast<Uint8>(c);
							if (dist == 0) {
								break;
							}
						}
					}
					dst[y * dstPitch + x] = bestIdx;
				}
			}
		}

		SDL_UnlockSurface(rgba);
		SDL_DestroySurface(rgba);
	}

	SDL_UnlockSurface(indexed);
	return indexed;
}

Uint32 Direct3DRMGlideRenderer::GetTextureId(IDirect3DRMTexture* iTexture, bool isUI, float scaleX, float scaleY)
{
	auto texture = static_cast<Direct3DRMTextureImpl*>(iTexture);
	auto surface = static_cast<DirectDrawSurfaceImpl*>(texture->m_surface);

	// Check if already cached
	for (Uint32 i = 0; i < m_textureCache.size(); ++i) {
		if (m_textureCache[i].texture == iTexture) {
			// Re-upload if version changed or was deferred (no palette at first call)
			if (m_textureCache[i].version != texture->m_version ||
				(m_textureCache[i].startAddress == 0xFFFFFFFF && m_palette)) {
				if (m_palette && surface->m_surface) {
					SDL_Surface* converted = ConvertToIndexed(surface->m_surface, m_palette);
					FxU32 addr = m_textureCache[i].startAddress;
					if (addr != 0xFFFFFFFF) {
						FxU32 tempAddr = addr;
						UploadGlideTexture(m_textureCache[i], converted, tempAddr);
						m_textureCache[i].startAddress = addr;
					}
					else {
						UploadGlideTexture(m_textureCache[i], converted, m_nextTextureAddress);
					}
					SDL_DestroySurface(converted);
				}
				m_textureCache[i].version = texture->m_version;
			}
			return i;
		}
	}

	// New texture
	GlideTextureEntry entry;
	memset(&entry, 0, sizeof(entry));
	entry.texture = iTexture;
	entry.version = texture->m_version;
	entry.startAddress = 0xFFFFFFFF;
	entry.texW = 0;
	entry.texH = 0;

	if (surface->m_surface && m_palette) {
		SDL_Surface* converted = ConvertToIndexed(surface->m_surface, m_palette);
		UploadGlideTexture(entry, converted, m_nextTextureAddress);
		SDL_DestroySurface(converted);
	}

	m_textureCache.push_back(entry);
	return static_cast<Uint32>(m_textureCache.size() - 1);
}

Uint32 Direct3DRMGlideRenderer::GetMeshId(IDirect3DRMMesh* mesh, const MeshGroup* meshGroup)
{
	for (Uint32 i = 0; i < m_meshCache.size(); ++i) {
		auto& cache = m_meshCache[i];
		if (cache.meshGroup == meshGroup) {
			if (cache.version != meshGroup->version) {
				cache = std::move(UploadMeshGlide(*meshGroup));
			}
			return i;
		}
	}

	auto newCache = UploadMeshGlide(*meshGroup);

	for (Uint32 i = 0; i < m_meshCache.size(); ++i) {
		auto& cache = m_meshCache[i];
		if (!cache.meshGroup) {
			cache = std::move(newCache);
			return i;
		}
	}

	m_meshCache.push_back(std::move(newCache));
	return static_cast<Uint32>(m_meshCache.size() - 1);
}

// Screen-space Sutherland-Hodgman clipping for GlideVertex polygons
static GlideVertex LerpGlideVertex(const GlideVertex& a, const GlideVertex& b, float t)
{
	GlideVertex r;
	r.x = a.x + t * (b.x - a.x);
	r.y = a.y + t * (b.y - a.y);
	r.ooz = a.ooz + t * (b.ooz - a.ooz);
	r.oow = a.oow + t * (b.oow - a.oow);
	r.r = a.r + t * (b.r - a.r);
	r.g = a.g + t * (b.g - a.g);
	r.b = a.b + t * (b.b - a.b);
	r.a = a.a + t * (b.a - a.a);
#ifdef GLIDE3
	r.sow = a.sow + t * (b.sow - a.sow);
	r.tow = a.tow + t * (b.tow - a.tow);
#else
	r.tmuvtx[0].sow = a.tmuvtx[0].sow + t * (b.tmuvtx[0].sow - a.tmuvtx[0].sow);
	r.tmuvtx[0].tow = a.tmuvtx[0].tow + t * (b.tmuvtx[0].tow - a.tmuvtx[0].tow);
#endif
	return r;
}

// Clip edges: 0=left, 1=right, 2=top, 3=bottom
static float ScreenEdgeDist(const GlideVertex& v, int edge, float minX, float maxX, float minY, float maxY)
{
	switch (edge) {
	case 0:
		return v.x - minX;
	case 1:
		return maxX - v.x;
	case 2:
		return v.y - minY;
	case 3:
		return maxY - v.y;
	default:
		return 0.0f;
	}
}

static int ClipGlidePolygonAgainstEdge(
	const GlideVertex* in,
	int inCount,
	GlideVertex* out,
	int edge,
	float minX,
	float maxX,
	float minY,
	float maxY
)
{
	if (inCount < 3) {
		return 0;
	}
	int outCount = 0;
	for (int i = 0; i < inCount; ++i) {
		const GlideVertex& cur = in[i];
		const GlideVertex& next = in[(i + 1) % inCount];
		float dCur = ScreenEdgeDist(cur, edge, minX, maxX, minY, maxY);
		float dNext = ScreenEdgeDist(next, edge, minX, maxX, minY, maxY);
		if (dCur >= 0) {
			out[outCount++] = cur;
			if (dNext < 0) {
				float t = dCur / (dCur - dNext);
				out[outCount++] = LerpGlideVertex(cur, next, t);
			}
		}
		else if (dNext >= 0) {
			float t = dCur / (dCur - dNext);
			out[outCount++] = LerpGlideVertex(cur, next, t);
		}
	}
	return outCount;
}

static int ClipGlidePolygonToScreen(
	GlideVertex* verts,
	int count,
	GlideVertex* temp,
	float minX,
	float maxX,
	float minY,
	float maxY
)
{
	for (int edge = 0; edge < 4; ++edge) {
		count = ClipGlidePolygonAgainstEdge(verts, count, temp, edge, minX, maxX, minY, maxY);
		if (count < 3) {
			return 0;
		}
		memcpy(verts, temp, count * sizeof(GlideVertex));
	}
	return count;
}

static void FillGlideVertex(
	GlideVertex& gv,
	float screenX,
	float screenY,
	float oow,
	float r,
	float g,
	float b,
	float a,
	float sow,
	float tow
)
{
	gv.x = screenX;
	gv.y = screenY;
	gv.ooz = 0; // not used in W-buffer mode
	gv.oow = oow;
	gv.r = r;
	gv.g = g;
	gv.b = b;
	gv.a = a;
#ifdef GLIDE3
	gv.sow = sow;
	gv.tow = tow;
#else
	gv.tmuvtx[0].sow = sow;
	gv.tmuvtx[0].tow = tow;
#endif
}

void Direct3DRMGlideRenderer::SubmitDraw(
	DWORD meshId,
	const D3DRMMATRIX4D& modelViewMatrix,
	const D3DRMMATRIX4D& worldMatrix,
	const D3DRMMATRIX4D& viewMatrix,
	const Matrix3x3& normalMatrix,
	const Appearance& appearance
)
{
	if (meshId >= m_meshCache.size()) {
		return;
	}
	auto& mesh = m_meshCache[meshId];

	// We need the original D3DRMVERTEX data - reconstruct from mesh group
	const MeshGroup* mg = mesh.meshGroup;
	if (!mg) {
		return;
	}

	// Get flat/smooth vertices - use cached data to avoid per-frame allocations
	const D3DRMVERTEX* cpuVerts;
	const uint16_t* flatIdx = nullptr;
	const DWORD* dwordIdx = nullptr;
	size_t vertCount, idxCount;

	if (mesh.flat) {
		cpuVerts = mesh.flatVertices.data();
		flatIdx = mesh.flatIndices.data();
		vertCount = mesh.flatVertices.size();
		idxCount = mesh.flatIndices.size();
	}
	else {
		cpuVerts = mg->vertices.data();
		dwordIdx = mg->indices.data();
		vertCount = mg->vertices.size();
		idxCount = mg->indices.size();
	}

	// Transform vertices to view space and pre-compute lighting
	m_transformedVertices.clear();
	m_transformedVertices.reserve(vertCount);
	m_litColors.clear();
	m_litColors.reserve(vertCount);
	for (size_t vi = 0; vi < vertCount; ++vi) {
		D3DRMVERTEX dst;
		dst.position = TransformPoint(cpuVerts[vi].position, modelViewMatrix);
		dst.normal = cpuVerts[vi].normal;
		dst.texCoord = cpuVerts[vi].texCoord;
		m_transformedVertices.push_back(dst);
		m_litColors.push_back(ApplyLighting(m_lights, dst.position, dst.normal, normalMatrix, appearance));
	}

	// Set up Glide texture combine mode
	bool hasTexture = (appearance.textureId != NO_TEXTURE_ID);
	float texW = 256.0f;
	float texH = 256.0f;

	if (hasTexture) {
		if (appearance.textureId >= m_textureCache.size()) {
			hasTexture = false;
		}
	}
	if (hasTexture) {
		auto& texEntry = m_textureCache[appearance.textureId];
		if (texEntry.startAddress == 0xFFFFFFFF) {
			hasTexture = false;
		}
		else {
			texW = static_cast<float>(texEntry.texW);
			texH = static_cast<float>(texEntry.texH);
			grTexSource(GR_TMU0, texEntry.startAddress, GR_MIPMAPLEVELMASK_BOTH, &texEntry.info);

			// Textured + lit: modulate texture by vertex color
			grColorCombine(
				GR_COMBINE_FUNCTION_SCALE_OTHER,
				GR_COMBINE_FACTOR_LOCAL,
				GR_COMBINE_LOCAL_ITERATED,
				GR_COMBINE_OTHER_TEXTURE,
				FXFALSE
			);
			grTexCombine(
				GR_TMU0,
				GR_COMBINE_FUNCTION_LOCAL,
				GR_COMBINE_FACTOR_NONE,
				GR_COMBINE_FUNCTION_LOCAL,
				GR_COMBINE_FACTOR_NONE,
				FXFALSE,
				FXFALSE
			);
			// Alpha from texture
			grAlphaCombine(
				GR_COMBINE_FUNCTION_SCALE_OTHER,
				GR_COMBINE_FACTOR_LOCAL,
				GR_COMBINE_LOCAL_ITERATED,
				GR_COMBINE_OTHER_TEXTURE,
				FXFALSE
			);
		}
	}
	if (!hasTexture) {
		// Untextured: vertex color only
		grColorCombine(
			GR_COMBINE_FUNCTION_LOCAL,
			GR_COMBINE_FACTOR_NONE,
			GR_COMBINE_LOCAL_ITERATED,
			GR_COMBINE_OTHER_NONE,
			FXFALSE
		);
		// Alpha from vertex
		grAlphaCombine(
			GR_COMBINE_FUNCTION_LOCAL,
			GR_COMBINE_FACTOR_NONE,
			GR_COMBINE_LOCAL_ITERATED,
			GR_COMBINE_OTHER_NONE,
			FXFALSE
		);
	}

	// Index accessor to handle different index types
	auto getIndex = [flatIdx, dwordIdx](size_t i) -> uint32_t {
		return flatIdx ? static_cast<uint32_t>(flatIdx[i]) : static_cast<uint32_t>(dwordIdx[i]);
	};

	// Process triangles
	for (size_t i = 0; i + 2 < idxCount; i += 3) {
		uint32_t idx0 = getIndex(i), idx1 = getIndex(i + 1), idx2 = getIndex(i + 2);
		D3DRMVERTEX v[3] = {
			m_transformedVertices[idx0],
			m_transformedVertices[idx1],
			m_transformedVertices[idx2],
		};

		// Backface culling in view space (same as software renderer)
		{
			D3DVECTOR e1 = {
				v[1].position.x - v[0].position.x,
				v[1].position.y - v[0].position.y,
				v[1].position.z - v[0].position.z
			};
			D3DVECTOR e2 = {
				v[2].position.x - v[0].position.x,
				v[2].position.y - v[0].position.y,
				v[2].position.z - v[0].position.z
			};
			D3DVECTOR normal = CrossProduct(e1, e2);
			if (DotProduct(normal, v[0].position) >= 0.0f) {
				continue;
			}
		}

		// Near-plane clip check (quick reject)
		if (v[0].position.z < m_frontClip && v[1].position.z < m_frontClip && v[2].position.z < m_frontClip) {
			continue;
		}
		if (v[0].position.z > m_backClip && v[1].position.z > m_backClip && v[2].position.z > m_backClip) {
			continue;
		}

		// Frustum side-plane quick reject
		if (IsTriangleOutsideViewCone(v[0].position, v[1].position, v[2].position, m_frustumPlanes)) {
			continue;
		}

		// Check if near-plane clipping is needed
		bool needsClip =
			(v[0].position.z < m_frontClip || v[1].position.z < m_frontClip || v[2].position.z < m_frontClip);

		if (!needsClip) {
			// Fast path: no clipping needed, use pre-lit colors
			GlideVertex grVerts[3];
			bool validTri = true;
			for (int j = 0; j < 3; ++j) {
				float sx, sy, sz, sw;
				ProjectVertex(m_projection, m_width, m_height, v[j].position, sx, sy, sz, sw);
				if (sw <= 0.001f) {
					validTri = false;
					break;
				}

				float oow = 1.0f / sw;
				SDL_Color litColor = m_litColors[getIndex(i + j)];

				float sow = 0.0f, tow = 0.0f;
				if (hasTexture) {
					sow = v[j].texCoord.u * texW * oow;
					tow = v[j].texCoord.v * texH * oow;
				}

				FillGlideVertex(
					grVerts[j],
					sx,
					sy,
					oow,
					static_cast<float>(litColor.r),
					static_cast<float>(litColor.g),
					static_cast<float>(litColor.b),
					static_cast<float>(litColor.a),
					sow,
					tow
				);
			}
			if (!validTri) {
				continue;
			}

			// Clip to screen bounds
			GlideVertex clipTemp[12];
			int polyCount =
				ClipGlidePolygonToScreen(grVerts, 3, clipTemp, 0.0f, (float) m_width, 0.0f, (float) m_height);
			if (polyCount < 3) {
				continue;
			}

			for (int j = 1; j < polyCount - 1; ++j) {
				grDrawTriangle(&grVerts[0], &grVerts[j], &grVerts[j + 1]);
			}
		}
		else {
			// Slow path: near-plane clipping generates new vertices, recompute lighting
			D3DRMVERTEX clipA[12], clipB[12];
			clipA[0] = v[0];
			clipA[1] = v[1];
			clipA[2] = v[2];
			int polyCount = 3;

			Plane nearPlane = {{0, 0, 1}, -m_frontClip};
			polyCount = ClipPolygonAgainstPlane(clipA, polyCount, clipB, nearPlane);
			if (polyCount < 3) {
				continue;
			}
			memcpy(clipA, clipB, polyCount * sizeof(D3DRMVERTEX));

			GlideVertex grVerts[12];
			bool validTri = true;
			for (int j = 0; j < polyCount; ++j) {
				float sx, sy, sz, sw;
				ProjectVertex(m_projection, m_width, m_height, clipA[j].position, sx, sy, sz, sw);
				if (sw <= 0.001f) {
					validTri = false;
					break;
				}

				float oow = 1.0f / sw;
				SDL_Color litColor =
					ApplyLighting(m_lights, clipA[j].position, clipA[j].normal, normalMatrix, appearance);

				float sow = 0.0f, tow = 0.0f;
				if (hasTexture) {
					sow = clipA[j].texCoord.u * texW * oow;
					tow = clipA[j].texCoord.v * texH * oow;
				}

				FillGlideVertex(
					grVerts[j],
					sx,
					sy,
					oow,
					static_cast<float>(litColor.r),
					static_cast<float>(litColor.g),
					static_cast<float>(litColor.b),
					static_cast<float>(litColor.a),
					sow,
					tow
				);
			}
			if (!validTri) {
				continue;
			}

			GlideVertex clipTemp[12];
			polyCount =
				ClipGlidePolygonToScreen(grVerts, polyCount, clipTemp, 0.0f, (float) m_width, 0.0f, (float) m_height);
			if (polyCount < 3) {
				continue;
			}

			for (int j = 1; j < polyCount - 1; ++j) {
				grDrawTriangle(&grVerts[0], &grVerts[j], &grVerts[j + 1]);
			}
		}
	}
}

void Direct3DRMGlideRenderer::Draw2DImage(
	Uint32 textureId,
	const SDL_Rect& srcRect,
	const SDL_Rect& dstRect,
	FColor color
)
{
	float x0 = dstRect.x * m_viewportTransform.scale + m_viewportTransform.offsetX;
	float y0 = dstRect.y * m_viewportTransform.scale + m_viewportTransform.offsetY;
	float x1 = x0 + dstRect.w * m_viewportTransform.scale;
	float y1 = y0 + dstRect.h * m_viewportTransform.scale;

	float r = color.r * 255.0f;
	float g = color.g * 255.0f;
	float b = color.b * 255.0f;
	float a = color.a * 255.0f;

	if (textureId == NO_TEXTURE_ID) {
		// Solid color quad
		grColorCombine(
			GR_COMBINE_FUNCTION_LOCAL,
			GR_COMBINE_FACTOR_NONE,
			GR_COMBINE_LOCAL_ITERATED,
			GR_COMBINE_OTHER_NONE,
			FXFALSE
		);

		grDepthBufferMode(GR_DEPTHBUFFER_DISABLE);

		GlideVertex v0, v1, v2, v3;
		memset(&v0, 0, sizeof(GlideVertex));
		memset(&v1, 0, sizeof(GlideVertex));
		memset(&v2, 0, sizeof(GlideVertex));
		memset(&v3, 0, sizeof(GlideVertex));

		FillGlideVertex(v0, x0, y0, 1.0f, r, g, b, a, 0, 0);
		FillGlideVertex(v1, x1, y0, 1.0f, r, g, b, a, 0, 0);
		FillGlideVertex(v2, x1, y1, 1.0f, r, g, b, a, 0, 0);
		FillGlideVertex(v3, x0, y1, 1.0f, r, g, b, a, 0, 0);

		grDrawTriangle(&v0, &v1, &v2);
		grDrawTriangle(&v0, &v2, &v3);

		grDepthBufferMode(GR_DEPTHBUFFER_WBUFFER);
		return;
	}

	if (textureId >= m_textureCache.size()) {
		return;
	}
	// Get the underlying surface for this texture to determine real dimensions
	auto& texEntry = m_textureCache[textureId];
	auto texture = static_cast<Direct3DRMTextureImpl*>(texEntry.texture);
	auto surface = static_cast<DirectDrawSurfaceImpl*>(texture->m_surface);
	SDL_Surface* src = surface->m_surface;

	if (!src) {
		return;
	}

	int imgW = src->w;
	int imgH = src->h;

	// For color-keyed 2D images, render as a P_8 textured quad with hardware chromakey.
	// This avoids RGB565 quantization issues that cause false color key matches.
	Uint32 ck = 0;
	bool hasCK = SDL_GetSurfaceColorKey(src, &ck);

	if (hasCK && src->format == SDL_PIXELFORMAT_INDEX8 && imgW <= 64 && imgH <= 64) {
		// Use power-of-2 square texture
		int texW = 1;
		while (texW < imgW) {
			texW <<= 1;
		}
		if (texW > 256) {
			texW = 256;
		}
		int texH = 1;
		while (texH < imgH) {
			texH <<= 1;
		}
		if (texH > 256) {
			texH = 256;
		}
		// Make square (use larger dimension)
		int texSize = std::max(texW, texH);

		// Allocate raw 8-bit texture data filled with colorkey index
		std::vector<Uint8> texData(texSize * texSize, (Uint8) ck);

		// Copy source pixels
		SDL_LockSurface(src);
		for (int row = 0; row < imgH; ++row) {
			Uint8* srcRow = static_cast<Uint8*>(src->pixels) + row * src->pitch;
			for (int col = 0; col < imgW; ++col) {
				texData[row * texSize + col] = srcRow[col];
			}
		}
		SDL_UnlockSurface(src);

		// Determine LOD
		GrLOD_t lod = GR_LOD_LOG2_256;
		if (texSize <= 128) {
			lod = GR_LOD_LOG2_128;
		}
		if (texSize <= 64) {
			lod = GR_LOD_LOG2_64;
		}
		if (texSize <= 32) {
			lod = GR_LOD_LOG2_32;
		}
		if (texSize <= 16) {
			lod = GR_LOD_LOG2_16;
		}
		if (texSize <= 8) {
			lod = GR_LOD_LOG2_8;
		}
		if (texSize <= 4) {
			lod = GR_LOD_LOG2_4;
		}
		if (texSize <= 2) {
			lod = GR_LOD_LOG2_2;
		}
		if (texSize <= 1) {
			lod = GR_LOD_LOG2_1;
		}

		GrTexInfo info;
		info.smallLodLog2 = lod;
		info.largeLodLog2 = lod;
		info.aspectRatioLog2 = GR_ASPECT_LOG2_1x1;
		info.format = GR_TEXFMT_P_8;
		info.data = texData.data();

		// Upload to start of texture memory as a transient texture.
		// This is safe because SubmitDraw always re-binds via grTexSource before 3D rendering.
		FxU32 texAddr = grTexMinAddress(GR_TMU0);
		grTexDownloadMipMap(GR_TMU0, texAddr, GR_MIPMAPLEVELMASK_BOTH, &info);
		grTexSource(GR_TMU0, texAddr, GR_MIPMAPLEVELMASK_BOTH, &info);

		// Set rendering state
		grDepthBufferMode(GR_DEPTHBUFFER_DISABLE);
		grColorCombine(
			GR_COMBINE_FUNCTION_SCALE_OTHER,
			GR_COMBINE_FACTOR_ONE,
			GR_COMBINE_LOCAL_NONE,
			GR_COMBINE_OTHER_TEXTURE,
			FXFALSE
		);
		grTexCombine(
			GR_TMU0,
			GR_COMBINE_FUNCTION_LOCAL,
			GR_COMBINE_FACTOR_NONE,
			GR_COMBINE_FUNCTION_LOCAL,
			GR_COMBINE_FACTOR_NONE,
			FXFALSE,
			FXFALSE
		);

		// Enable chroma key
		grChromakeyMode(GR_CHROMAKEY_ENABLE);
		SDL_Palette* pal = m_palette ? m_palette : SDL_GetSurfacePalette(src);
		Uint8 ckIdx = (Uint8) ck;
		GrColor_t ckColor = 0;
		if (pal && ckIdx < pal->ncolors) {
			ckColor = ((FxU32) pal->colors[ckIdx].r << 16) | ((FxU32) pal->colors[ckIdx].g << 8) |
					  ((FxU32) pal->colors[ckIdx].b);
		}
		grChromakeyValue(ckColor);

		// Draw quad
		float qx0 = dstRect.x * m_viewportTransform.scale + m_viewportTransform.offsetX;
		float qy0 = dstRect.y * m_viewportTransform.scale + m_viewportTransform.offsetY;
		float qx1 = qx0 + dstRect.w * m_viewportTransform.scale;
		float qy1 = qy0 + dstRect.h * m_viewportTransform.scale;
		float halfTexel = 256.0f * 0.5f / static_cast<float>(texSize);
		float s0 = 256.0f * static_cast<float>(srcRect.x) / static_cast<float>(texSize);
		float t0 = 256.0f * static_cast<float>(srcRect.y) / static_cast<float>(texSize) + halfTexel;
		float s1 = 256.0f * static_cast<float>(srcRect.x + srcRect.w) / static_cast<float>(texSize);
		float t1 = 256.0f * static_cast<float>(srcRect.y + srcRect.h) / static_cast<float>(texSize) + halfTexel;

		GlideVertex gv0, gv1, gv2, gv3;
		memset(&gv0, 0, sizeof(GlideVertex));
		memset(&gv1, 0, sizeof(GlideVertex));
		memset(&gv2, 0, sizeof(GlideVertex));
		memset(&gv3, 0, sizeof(GlideVertex));

		FillGlideVertex(gv0, qx0, qy0, 1.0f, 255, 255, 255, 255, s0, t0);
		FillGlideVertex(gv1, qx1, qy0, 1.0f, 255, 255, 255, 255, s1, t0);
		FillGlideVertex(gv2, qx1, qy1, 1.0f, 255, 255, 255, 255, s1, t1);
		FillGlideVertex(gv3, qx0, qy1, 1.0f, 255, 255, 255, 255, s0, t1);

		grDrawTriangle(&gv0, &gv1, &gv2);
		grDrawTriangle(&gv0, &gv2, &gv3);

		// Restore state
		grChromakeyMode(GR_CHROMAKEY_DISABLE);
		grDepthBufferMode(GR_DEPTHBUFFER_WBUFFER);
		return;
	}

	// LFB (linear framebuffer) direct write for 2D images.
	{
		SDL_SetSurfaceColorKey(src, false, 0);
		SDL_Surface* converted = SDL_ConvertSurface(src, SDL_PIXELFORMAT_RGB565);
		if (hasCK) {
			SDL_SetSurfaceColorKey(src, true, ck);
		}
		if (!converted) {
			return;
		}

		// For color-keyed surfaces, we need to identify which pixels are transparent.
		// The palette may have duplicate black entries, so we mark the keyed pixels
		// with a unique sentinel value (0xF81F = magenta in RGB565) before writing.
		Uint16 sentinel = 0xF81F; // bright magenta - unlikely to appear naturally
		if (hasCK && src->format == SDL_PIXELFORMAT_INDEX8) {
			// Re-scan original paletted source to mark keyed pixels in converted surface
			SDL_LockSurface(src);
			SDL_LockSurface(converted);
			for (int row = 0; row < imgH; ++row) {
				Uint8* srcRow = static_cast<Uint8*>(src->pixels) + row * src->pitch;
				Uint16* dstRow =
					reinterpret_cast<Uint16*>(static_cast<Uint8*>(converted->pixels) + row * converted->pitch);
				for (int col = 0; col < imgW; ++col) {
					if (srcRow[col] == (Uint8) ck) {
						dstRow[col] = sentinel;
					}
				}
			}
			SDL_UnlockSurface(src);
			SDL_UnlockSurface(converted);
		}

		// Calculate destination region on the 640x480 framebuffer
		int dstX = static_cast<int>(x0);
		int dstY = static_cast<int>(y0);
		int dstW = static_cast<int>(x1 - x0);
		int dstH = static_cast<int>(y1 - y0);

		// Prepare source at the correct size
		SDL_Surface* blitSrc = converted;
		SDL_Surface* scaled = nullptr;
		if (dstW != srcRect.w || dstH != srcRect.h) {
			scaled = SDL_CreateSurface(dstW, dstH, SDL_PIXELFORMAT_RGB565);
			if (scaled) {
				SDL_Rect sr = srcRect;
				SDL_Rect dr = {0, 0, dstW, dstH};
				SDL_BlitSurfaceScaled(converted, &sr, scaled, &dr, SDL_SCALEMODE_NEAREST);
				blitSrc = scaled;
			}
		}

		Uint16* srcPixels;
		int srcPitch;
		if (blitSrc == converted) {
			srcPixels = reinterpret_cast<Uint16*>(
				static_cast<Uint8*>(converted->pixels) + srcRect.y * converted->pitch + srcRect.x * 2
			);
			srcPitch = converted->pitch;
		}
		else {
			srcPixels = reinterpret_cast<Uint16*>(blitSrc->pixels);
			srcPitch = blitSrc->pitch;
		}

		int writeW = blitSrc == converted ? srcRect.w : dstW;
		int writeH = blitSrc == converted ? srcRect.h : dstH;

		if (!hasCK) {
			grLfbWriteRegion(
				GR_BUFFER_BACKBUFFER,
				dstX,
				dstY,
				GR_LFB_SRC_FMT_565,
				writeW,
				writeH,
#ifdef GLIDE3
				FXFALSE,
#endif
				srcPitch,
				srcPixels
			);
		}
		else {
			// Read framebuffer, composite skipping sentinel, write back
			int clipX = std::max(0, dstX);
			int clipY = std::max(0, dstY);
			int clipW = std::min(writeW, m_width - clipX);
			int clipH = std::min(writeH, m_height - clipY);
			int srcOffX = clipX - dstX;
			int srcOffY = clipY - dstY;

			if (clipW > 0 && clipH > 0) {
				std::vector<Uint16> fbRegion(clipW * clipH);
				grLfbReadRegion(GR_BUFFER_BACKBUFFER, clipX, clipY, clipW, clipH, clipW * 2, fbRegion.data());

				int srcStride = srcPitch / 2;
				for (int row = 0; row < clipH; ++row) {
					for (int col = 0; col < clipW; ++col) {
						Uint16 px = srcPixels[(srcOffY + row) * srcStride + (srcOffX + col)];
						if (px != sentinel) {
							fbRegion[row * clipW + col] = px;
						}
					}
				}

				grLfbWriteRegion(
					GR_BUFFER_BACKBUFFER,
					clipX,
					clipY,
					GR_LFB_SRC_FMT_565,
					clipW,
					clipH,
#ifdef GLIDE3
					FXFALSE,
#endif
					clipW * 2,
					fbRegion.data()
				);
			}
		}

		if (scaled) {
			SDL_DestroySurface(scaled);
		}
		SDL_DestroySurface(converted);
	}
}

// ---------------------------------------------------------------------------
// Framebuffer readback
// ---------------------------------------------------------------------------

void Direct3DRMGlideRenderer::Download(SDL_Surface* target)
{
	if (!target) {
		return;
	}

	int srcX = static_cast<int>(m_viewportTransform.offsetX);
	int srcY = static_cast<int>(m_viewportTransform.offsetY);
	int srcW = static_cast<int>(m_virtualWidth * m_viewportTransform.scale);
	int srcH = static_cast<int>(m_virtualHeight * m_viewportTransform.scale);

	// Allocate temporary buffer for Glide LFB read (16-bit RGB565)
	std::vector<Uint16> lfbBuffer(m_width * m_height);

	grLfbReadRegion(GR_BUFFER_BACKBUFFER, 0, 0, m_width, m_height, m_width * 2, lfbBuffer.data());

	// Create a temporary SDL surface from the LFB data
	SDL_Surface* glideSurface =
		SDL_CreateSurfaceFrom(m_width, m_height, SDL_PIXELFORMAT_RGB565, lfbBuffer.data(), m_width * 2);

	if (glideSurface) {
		SDL_Rect srcRect = {srcX, srcY, srcW, srcH};
		SDL_BlitSurfaceScaled(glideSurface, &srcRect, target, nullptr, SDL_SCALEMODE_LINEAR);
		SDL_DestroySurface(glideSurface);
	}
}
