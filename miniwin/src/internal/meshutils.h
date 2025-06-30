#pragma once

#include "miniwin/d3drm.h"

#include <vector>

void FlattenSurfaces(
	const D3DRMVERTEX* vertices,
	const size_t vertexCount,
	const DWORD* indices,
	const size_t indexCount,
	bool hasTexture,
	std::vector<D3DRMVERTEX>& dedupedVertices,
	std::vector<uint16_t>& newIndices
);

void Create2DTransformMatrix(
	const SDL_Rect& dstRect,
	float scale,
	float offsetX,
	float offsetY,
	D3DRMMATRIX4D& outMatrix
);

void CreateOrthographicProjection(float width, float height, D3DRMMATRIX4D& outProj);
