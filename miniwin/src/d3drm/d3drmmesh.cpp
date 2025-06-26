#include "d3drmmesh_impl.h"
#include "miniwin.h"

#include <limits>

HRESULT Direct3DRMMeshImpl::QueryInterface(const GUID& riid, void** ppvObject)
{
	if (SDL_memcmp(&riid, &IID_IDirect3DRMMesh, sizeof(GUID)) == 0) {
		this->IUnknown::AddRef();
		*ppvObject = static_cast<IDirect3DRMMesh*>(this);
		return S_OK;
	}
	if (SDL_memcmp(&riid, &IID_IDirect3DRMFrame, sizeof(GUID)) == 0) {
		return E_NOINTERFACE;
	}
	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Direct3DRMMeshImpl does not implement guid");
	return E_NOINTERFACE;
}

HRESULT Direct3DRMMeshImpl::Clone(int flags, GUID iid, void** object)
{
	if (!object || SDL_memcmp(&iid, &IID_IDirect3DRMMesh, sizeof(GUID)) != 0) {
		return DDERR_INVALIDPARAMS;
	}

	auto* clone = new Direct3DRMMeshImpl(*this);

	for (auto& group : clone->m_groups) {
		// Reusing the same texture and material on the new mesh instead of cloning them might not be correct
		if (group.texture) {
			group.texture->AddRef();
		}
		if (group.material) {
			group.material->AddRef();
		}
	}

	*object = static_cast<IDirect3DRMMesh*>(clone);
	return DD_OK;
}

HRESULT Direct3DRMMeshImpl::AddGroup(
	unsigned int vertexCount,
	unsigned int faceCount,
	unsigned int vertexPerFace,
	unsigned int* faceBuffer,
	D3DRMGROUPINDEX* groupIndex
)
{
	if (faceCount < 0 || vertexPerFace < 0 || !faceBuffer) {
		return DDERR_INVALIDPARAMS;
	}

	int newIndex = m_groups.size();
	if (groupIndex) {
		*groupIndex = newIndex;
	}

	MeshGroup group;
	group.vertexPerFace = vertexPerFace;

	unsigned int* src = faceBuffer;
	group.indices.assign(src, src + faceCount * vertexPerFace);

	m_groups.push_back(std::move(group));

	return DD_OK;
}

HRESULT Direct3DRMMeshImpl::GetGroup(
	D3DRMGROUPINDEX groupIndex,
	unsigned int* vertexCount,
	unsigned int* faceCount,
	unsigned int* vertexPerFace,
	DWORD* indexCount,
	unsigned int* indices
)
{
	if (groupIndex >= m_groups.size()) {
		return DDERR_INVALIDPARAMS;
	}

	const auto& group = m_groups[groupIndex];

	if (vertexCount) {
		*vertexCount = static_cast<DWORD>(group.vertices.size());
	}
	if (faceCount) {
		*faceCount = static_cast<DWORD>(group.indices.size() / group.vertexPerFace);
	}
	if (vertexPerFace) {
		*vertexPerFace = static_cast<DWORD>(group.vertexPerFace);
	}
	if (indexCount) {
		*indexCount = static_cast<DWORD>(group.indices.size());
	}
	if (indices) {
		std::copy(group.indices.begin(), group.indices.end(), reinterpret_cast<unsigned int*>(indices));
	}

	return DD_OK;
}

const MeshGroup& Direct3DRMMeshImpl::GetGroup(D3DRMGROUPINDEX groupIndex)
{
	return m_groups[groupIndex];
}

DWORD Direct3DRMMeshImpl::GetGroupCount()
{
	return m_groups.size();
}

HRESULT Direct3DRMMeshImpl::SetGroupColor(D3DRMGROUPINDEX groupIndex, D3DCOLOR color)
{
	if (groupIndex >= m_groups.size()) {
		return DDERR_INVALIDPARAMS;
	}

	m_groups[groupIndex].color = {
		static_cast<Uint8>((color >> 16) & 0xFF),
		static_cast<Uint8>((color >> 8) & 0xFF),
		static_cast<Uint8>((color >> 0) & 0xFF),
		static_cast<Uint8>((color >> 24) & 0xFF)
	};
	return DD_OK;
}

HRESULT Direct3DRMMeshImpl::SetGroupColorRGB(D3DRMGROUPINDEX groupIndex, float r, float g, float b)
{
	if (groupIndex >= m_groups.size()) {
		return DDERR_INVALIDPARAMS;
	}

	m_groups[groupIndex]
		.color = {static_cast<Uint8>(r * 255.0f), static_cast<Uint8>(g * 255.0f), static_cast<Uint8>(b * 255.0f), 255};

	return DD_OK;
}

D3DCOLOR Direct3DRMMeshImpl::GetGroupColor(D3DRMGROUPINDEX index)
{
	if (index < 0 || index >= static_cast<int>(m_groups.size())) {
		return 0xFFFFFFFF;
	}

	const SDL_Color& color = m_groups[index].color;
	return (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
}

HRESULT Direct3DRMMeshImpl::SetGroupMaterial(D3DRMGROUPINDEX groupIndex, IDirect3DRMMaterial* material)
{
	if (groupIndex >= m_groups.size()) {
		return DDERR_INVALIDPARAMS;
	}

	auto& group = m_groups[groupIndex];
	if (group.material) {
		group.material->Release();
	}

	material->AddRef();
	m_groups[groupIndex].material = material;
	return DD_OK;
}

HRESULT Direct3DRMMeshImpl::SetGroupTexture(D3DRMGROUPINDEX groupIndex, IDirect3DRMTexture* texture)
{
	if (groupIndex >= m_groups.size()) {
		return DDERR_INVALIDPARAMS;
	}

	auto& group = m_groups[groupIndex];
	if (group.texture) {
		group.texture->Release();
	}

	texture->AddRef();
	group.texture = texture;
	group.version++;
	return DD_OK;
}

HRESULT Direct3DRMMeshImpl::GetGroupMaterial(D3DRMGROUPINDEX groupIndex, LPDIRECT3DRMMATERIAL* material)
{
	if (groupIndex >= m_groups.size()) {
		return DDERR_GENERIC;
	}

	auto& group = m_groups[groupIndex];
	if (!group.material) {
		return DDERR_GENERIC;
	}

	group.material->AddRef();
	*material = group.material;
	return DD_OK;
}

HRESULT Direct3DRMMeshImpl::GetGroupTexture(D3DRMGROUPINDEX groupIndex, LPDIRECT3DRMTEXTURE* texture)
{
	if (groupIndex >= m_groups.size()) {
		return DDERR_GENERIC;
	}

	auto& group = m_groups[groupIndex];
	if (!group.texture) {
		return DDERR_GENERIC;
	}

	group.texture->AddRef();
	*texture = group.texture;
	return DD_OK;
}

HRESULT Direct3DRMMeshImpl::SetGroupMapping(D3DRMGROUPINDEX groupIndex, D3DRMMAPPING mapping)
{
	return DD_OK;
}

D3DRMMAPPING Direct3DRMMeshImpl::GetGroupMapping(D3DRMGROUPINDEX groupIndex)
{
	return D3DRMMAP_PERSPCORRECT;
}

HRESULT Direct3DRMMeshImpl::SetGroupQuality(D3DRMGROUPINDEX groupIndex, D3DRMRENDERQUALITY quality)
{
	if (groupIndex >= m_groups.size()) {
		return DDERR_INVALIDPARAMS;
	}

	switch (quality) {
	case D3DRMRENDER_WIREFRAME:
	case D3DRMRENDER_UNLITFLAT:
		MINIWIN_NOT_IMPLEMENTED();
		break;
	}

	auto& group = m_groups[groupIndex];
	group.quality = quality;
	group.version++;

	return DD_OK;
}

D3DRMRENDERQUALITY Direct3DRMMeshImpl::GetGroupQuality(D3DRMGROUPINDEX groupIndex)
{
	if (groupIndex >= m_groups.size()) {
		return D3DRMRENDER_GOURAUD;
	}

	return m_groups[groupIndex].quality;
}

HRESULT Direct3DRMMeshImpl::SetVertices(D3DRMGROUPINDEX groupIndex, int offset, int count, D3DRMVERTEX* vertices)
{
	if (count <= 0 || offset < 0 || groupIndex < 0 || groupIndex >= static_cast<int>(m_groups.size())) {
		return DDERR_INVALIDPARAMS;
	}

	auto& group = m_groups[groupIndex];
	auto& vertList = group.vertices;

	if (offset + count > static_cast<int>(vertList.size())) {
		vertList.resize(offset + count);
	}

	std::copy(vertices, vertices + count, vertList.begin() + offset);

	UpdateBox();

	group.version++;

	return DD_OK;
}

HRESULT Direct3DRMMeshImpl::GetVertices(D3DRMGROUPINDEX groupIndex, int startIndex, int count, D3DRMVERTEX* vertices)
{
	if (count <= 0 || startIndex < 0 || groupIndex < 0 || groupIndex >= static_cast<int>(m_groups.size())) {
		return DDERR_INVALIDPARAMS;
	}

	const auto& vertList = m_groups[groupIndex].vertices;

	if (startIndex + count > static_cast<int>(vertList.size())) {
		return DDERR_INVALIDPARAMS;
	}

	std::copy(vertList.begin() + startIndex, vertList.begin() + startIndex + count, vertices);
	return DD_OK;
}

void Direct3DRMMeshImpl::UpdateBox()
{
	const float INF = std::numeric_limits<float>::max();
	m_box.min = {INF, INF, INF};
	m_box.max = {-INF, -INF, -INF};

	for (size_t i = 0; i < m_groups.size(); ++i) {
		for (const D3DRMVERTEX& v : m_groups[i].vertices) {
			m_box.min.x = std::min(m_box.min.x, v.position.x);
			m_box.min.y = std::min(m_box.min.y, v.position.y);
			m_box.min.z = std::min(m_box.min.z, v.position.z);
			m_box.max.x = std::max(m_box.max.x, v.position.x);
			m_box.max.y = std::max(m_box.max.y, v.position.y);
			m_box.max.z = std::max(m_box.max.z, v.position.z);
		}
	}
}

HRESULT Direct3DRMMeshImpl::GetBox(D3DRMBOX* box)
{
	*box = m_box;

	return DD_OK;
}
