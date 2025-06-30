#include "meshutils.h"

#include "mathutils.h"

#include <tuple>
#include <unordered_map>

bool operator==(const D3DRMVERTEX& a, const D3DRMVERTEX& b)
{
	return memcmp(&a, &b, sizeof(D3DRMVERTEX)) == 0;
}

namespace std
{
template <>
struct hash<D3DRMVERTEX> {
	size_t operator()(const D3DRMVERTEX& v) const
	{
		const float* f = reinterpret_cast<const float*>(&v);
		size_t h = 0;
		for (int i = 0; i < sizeof(D3DRMVERTEX) / sizeof(float); ++i) {
			h ^= std::hash<float>()(f[i]) + 0x9e3779b9 + (h << 6) + (h >> 2);
		}
		return h;
	}
};
} // namespace std

D3DVECTOR ComputeTriangleNormal(const D3DVECTOR& v0, const D3DVECTOR& v1, const D3DVECTOR& v2)
{
	D3DVECTOR u = {v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
	D3DVECTOR v = {v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};
	D3DVECTOR normal = CrossProduct(u, v);
	normal = Normalize(normal);

	return normal;
}

void FlattenSurfaces(
	const D3DRMVERTEX* vertices,
	const size_t vertexCount,
	const DWORD* indices,
	const size_t indexCount,
	bool hasTexture,
	std::vector<D3DRMVERTEX>& dedupedVertices,
	std::vector<uint16_t>& newIndices
)
{
	std::unordered_map<D3DRMVERTEX, DWORD> uniqueVertexMap;

	dedupedVertices.reserve(vertexCount);
	newIndices.reserve(indexCount);

	for (size_t i = 0; i < indexCount; i += 3) {
		D3DRMVERTEX v0 = vertices[indices[i + 0]];
		D3DRMVERTEX v1 = vertices[indices[i + 1]];
		D3DRMVERTEX v2 = vertices[indices[i + 2]];
		v0.normal = v1.normal = v2.normal = ComputeTriangleNormal(v0.position, v1.position, v2.position);
		if (!hasTexture) {
			v0.texCoord = v1.texCoord = v2.texCoord = {0.0f, 0.0f};
		}

		// Deduplicate vertecies
		for (const D3DRMVERTEX& v : {v0, v1, v2}) {
			auto it = uniqueVertexMap.find(v);
			if (it != uniqueVertexMap.end()) {
				newIndices.push_back(it->second);
			}
			else {
				DWORD newIndex = static_cast<DWORD>(dedupedVertices.size());
				uniqueVertexMap[v] = newIndex;
				dedupedVertices.push_back(v);
				newIndices.push_back(newIndex);
			}
		}
	}
}

void Create2DTransformMatrix(
	const SDL_Rect& dstRect,
	float scale,
	float offsetX,
	float offsetY,
	D3DRMMATRIX4D& outMatrix
)
{
	float x = static_cast<float>(dstRect.x) * scale + offsetX;
	float y = static_cast<float>(dstRect.y) * scale + offsetY;
	float w = static_cast<float>(dstRect.w) * scale;
	float h = static_cast<float>(dstRect.h) * scale;

	D3DVALUE tmp[4][4] = {{w, 0, 0, 0}, {0, h, 0, 0}, {0, 0, 1, 0}, {x, y, 0, 1}};
	memcpy(outMatrix, tmp, sizeof(tmp));
}

void CreateOrthographicProjection(float width, float height, D3DRMMATRIX4D& outProj)
{
	D3DVALUE tmp[4][4] = {
		{2.0f / width, 0.0f, 0.0f, 0.0f},
		{0.0f, -2.0f / height, 0.0f, 0.0f},
		{0.0f, 0.0f, 1.0f, 0.0f},
		{-1.0f, 1.0f, 0.0f, 1.0f}
	};
	memcpy(outProj, tmp, sizeof(tmp));
}
