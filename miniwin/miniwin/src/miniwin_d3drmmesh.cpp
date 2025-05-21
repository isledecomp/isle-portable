#include "miniwin_d3drmmesh_p.h"
#include "miniwin_p.h"

HRESULT Direct3DRMMeshImpl::Clone(int flags, GUID iid, void** object)
{
	if (SDL_memcmp(&iid, &IID_IDirect3DRMMesh, sizeof(GUID)) == 0) {
		*object = static_cast<IDirect3DRMMesh*>(new Direct3DRMMeshImpl);
		return DD_OK;
	}

	return DDERR_GENERIC;
}

HRESULT Direct3DRMMeshImpl::GetBox(D3DRMBOX* box)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMMeshImpl::AddGroup(
	int vertexCount,
	int faceCount,
	int vertexPerFace,
	void* faceBuffer,
	D3DRMGROUPINDEX* groupIndex
)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMMeshImpl::GetGroup(
	int groupIndex,
	DWORD* vertexCount,
	DWORD* faceCount,
	DWORD* vertexPerFace,
	DWORD* dataSize,
	DWORD* data
)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMMeshImpl::SetGroupColor(int groupIndex, D3DCOLOR color)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMMeshImpl::SetGroupColorRGB(int groupIndex, float r, float g, float b)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMMeshImpl::SetGroupMaterial(int groupIndex, IDirect3DRMMaterial* material)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMMeshImpl::SetGroupMapping(D3DRMGROUPINDEX groupIndex, D3DRMMAPPING mapping)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMMeshImpl::SetGroupQuality(int groupIndex, D3DRMRENDERQUALITY quality)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMMeshImpl::SetVertices(int groupIndex, int offset, int count, D3DRMVERTEX* vertices)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMMeshImpl::SetGroupTexture(int groupIndex, IDirect3DRMTexture* texture)
{
	MINIWIN_NOT_IMPLEMENTED();
	m_groupTexture = texture;
	return DD_OK;
}

HRESULT Direct3DRMMeshImpl::GetGroupTexture(int groupIndex, LPDIRECT3DRMTEXTURE* texture)
{
	MINIWIN_NOT_IMPLEMENTED();
	if (!m_groupTexture) {
		return DDERR_GENERIC;
	}

	m_groupTexture->AddRef();
	*texture = m_groupTexture;
	return DD_OK;
}

D3DRMMAPPING Direct3DRMMeshImpl::GetGroupMapping(int groupIndex)
{
	MINIWIN_NOT_IMPLEMENTED();
	return D3DRMMAP_PERSPCORRECT;
}

D3DRMRENDERQUALITY Direct3DRMMeshImpl::GetGroupQuality(int groupIndex)
{
	MINIWIN_NOT_IMPLEMENTED();
	return D3DRMRENDER_GOURAUD;
}

HRESULT Direct3DRMMeshImpl::GetGroupColor(D3DRMGROUPINDEX index)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMMeshImpl::GetVertices(int groupIndex, int startIndex, int count, D3DRMVERTEX* vertices)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}
