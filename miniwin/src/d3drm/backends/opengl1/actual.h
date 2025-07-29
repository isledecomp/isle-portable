#pragma once

#include "structs.h"

#include <SDL3/SDL.h>
#include <stdint.h>
#include <vector>

// We don't want to transitively include windows.h, but we need GLuint
typedef unsigned int GLuint;
struct IDirect3DRMTexture;
struct MeshGroup;

typedef float Matrix4x4[4][4];

struct GL11_BridgeVector {
	float x, y, z;
};

struct GL11_BridgeTexCoord {
	float u, v;
};

struct GL11_BridgeSceneLight {
	FColor color;
	GL11_BridgeVector position;
	float positional;
	GL11_BridgeVector direction;
	float directional;
};

struct GL11_BridgeSceneVertex {
	GL11_BridgeVector position;
	GL11_BridgeVector normal;
	float tu, tv;
};

struct GLTextureCacheEntry {
	IDirect3DRMTexture* texture;
	Uint32 version;
	GLuint glTextureId;
	float width;
	float height;
};

struct GLMeshCacheEntry {
	const MeshGroup* meshGroup;
	int version;
	bool flat;

	// non-VBO cache
	std::vector<GL11_BridgeVector> positions;
	std::vector<GL11_BridgeVector> normals;
	std::vector<GL11_BridgeTexCoord> texcoords;
	std::vector<uint16_t> indices;

	// VBO cache
	GLuint vboPositions;
	GLuint vboNormals;
	GLuint vboTexcoords;
	GLuint ibo;
};

void GL11_InitState();
void GL11_LoadExtensions();
void GL11_DestroyTexture(GLuint texId);
int GL11_GetMaxTextureSize();
GLuint GL11_UploadTextureData(void* pixels, int width, int height, bool isUI, float scaleX, float scaleY);
void GL11_UploadMesh(GLMeshCacheEntry& cache, bool hasTexture);
void GL11_DestroyMesh(GLMeshCacheEntry& cache);
void GL11_BeginFrame(const Matrix4x4* projection);
void GL11_UploadLight(int lightIdx, GL11_BridgeSceneLight* l);
void GL11_EnableTransparency();
void GL11_SubmitDraw(
	GLMeshCacheEntry& mesh,
	const Matrix4x4& modelViewMatrix,
	const Appearance& appearance,
	GLuint texId
);
void GL11_Resize(int width, int height);
void GL11_Clear(float r, float g, float b);
void GL11_Draw2DImage(
	const GLTextureCacheEntry* cache,
	const SDL_Rect& srcRect,
	const SDL_Rect& dstRect,
	const FColor& color,
	float left,
	float right,
	float bottom,
	float top
);
void GL11_SetDither(bool dither);
void GL11_Download(SDL_Surface* target);
