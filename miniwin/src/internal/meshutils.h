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
