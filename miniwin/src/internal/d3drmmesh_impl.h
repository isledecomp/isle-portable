#pragma once

#include "d3drmobject_impl.h"

#include <algorithm>
#include <vector>

struct MeshGroup {
	D3DCOLOR color = 0xFFFFFFFF;
	IDirect3DRMTexture* texture = nullptr;
	IDirect3DRMMaterial* material = nullptr;
	D3DRMRENDERQUALITY quality = D3DRMRENDER_GOURAUD;
	int vertexPerFace = 0;
	std::vector<D3DRMVERTEX> vertices;
	std::vector<unsigned int> indices;

	MeshGroup() = default;

	MeshGroup(const MeshGroup& other)
		: color(other.color), texture(other.texture), material(other.material), quality(other.quality),
		  vertexPerFace(other.vertexPerFace), vertices(std::move(other.vertices)), indices(std::move(other.indices))
	{
		if (texture) {
			texture->AddRef();
		}
		if (material) {
			material->AddRef();
		}
	}

	// Move constructor
	MeshGroup(MeshGroup&& other) noexcept
		: color(other.color), texture(other.texture), material(other.material), quality(other.quality),
		  vertexPerFace(other.vertexPerFace), vertices(other.vertices), indices(other.indices)
	{
		other.texture = nullptr;
		other.material = nullptr;
	}

	// Move assignment
	MeshGroup& operator=(MeshGroup&& other) noexcept
	{
		color = other.color;
		texture = other.texture;
		material = other.material;
		quality = other.quality;
		vertexPerFace = other.vertexPerFace;
		vertices = std::move(other.vertices);
		indices = std::move(other.indices);
		other.texture = nullptr;
		other.material = nullptr;
		return *this;
	}

	~MeshGroup()
	{
		if (texture) {
			texture->Release();
		}
		if (material) {
			material->Release();
		}
	}
};

struct Direct3DRMMeshImpl : public Direct3DRMObjectBaseImpl<IDirect3DRMMesh> {
	HRESULT QueryInterface(const GUID& riid, void** ppvObject) override;
	HRESULT Clone(int flags, GUID iid, void** object) override;
	HRESULT AddGroup(int vertexCount, int faceCount, int vertexPerFace, DWORD* faceBuffer, D3DRMGROUPINDEX* groupIndex)
		override;
	HRESULT GetGroup(
		DWORD groupIndex,
		DWORD* vertexCount,
		DWORD* faceCount,
		DWORD* vertexPerFace,
		DWORD* indexCount,
		DWORD* indices
	) override;
	DWORD GetGroupCount() override;
	HRESULT SetGroupColor(DWORD groupIndex, D3DCOLOR color) override;
	HRESULT SetGroupColorRGB(DWORD groupIndex, float r, float g, float b) override;
	D3DCOLOR GetGroupColor(D3DRMGROUPINDEX index) override;
	HRESULT SetGroupMaterial(DWORD groupIndex, IDirect3DRMMaterial* material) override;
	HRESULT SetGroupTexture(DWORD groupIndex, IDirect3DRMTexture* texture) override;
	HRESULT GetGroupTexture(DWORD groupIndex, LPDIRECT3DRMTEXTURE* texture) override;
	HRESULT GetGroupMaterial(DWORD groupIndex, LPDIRECT3DRMMATERIAL* material) override;
	HRESULT SetGroupMapping(D3DRMGROUPINDEX groupIndex, D3DRMMAPPING mapping) override;
	D3DRMMAPPING GetGroupMapping(DWORD groupIndex) override;
	HRESULT SetGroupQuality(DWORD groupIndex, D3DRMRENDERQUALITY quality) override;
	D3DRMRENDERQUALITY GetGroupQuality(DWORD groupIndex) override;
	HRESULT SetVertices(DWORD groupIndex, int offset, int count, D3DRMVERTEX* vertices) override;
	HRESULT GetVertices(DWORD groupIndex, int startIndex, int count, D3DRMVERTEX* vertices) override;
	HRESULT GetBox(D3DRMBOX* box) override;

private:
	void UpdateBox();

	std::vector<MeshGroup> m_groups;
	D3DRMBOX m_box;
};
