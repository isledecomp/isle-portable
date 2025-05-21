#pragma once

#include "miniwin_d3drmobject_p.h"

struct Direct3DRMMeshImpl : public Direct3DRMObjectBase<IDirect3DRMMesh> {
	HRESULT Clone(int flags, GUID iid, void** object) override;
	HRESULT GetBox(D3DRMBOX* box) override;
	HRESULT AddGroup(int vertexCount, int faceCount, int vertexPerFace, void* faceBuffer, D3DRMGROUPINDEX* groupIndex)
		override;
	HRESULT GetGroup(
		int groupIndex,
		DWORD* vertexCount,
		DWORD* faceCount,
		DWORD* vertexPerFace,
		DWORD* dataSize,
		DWORD* data
	) override;
	HRESULT SetGroupColor(int groupIndex, D3DCOLOR color) override;
	HRESULT SetGroupColorRGB(int groupIndex, float r, float g, float b) override;
	HRESULT SetGroupMaterial(int groupIndex, IDirect3DRMMaterial* material) override;
	HRESULT SetGroupMapping(D3DRMGROUPINDEX groupIndex, D3DRMMAPPING mapping) override;
	HRESULT SetGroupQuality(int groupIndex, D3DRMRENDERQUALITY quality) override;
	HRESULT SetVertices(int groupIndex, int offset, int count, D3DRMVERTEX* vertices) override;
	HRESULT SetGroupTexture(int groupIndex, IDirect3DRMTexture* texture) override;
	;
	HRESULT GetGroupTexture(int groupIndex, LPDIRECT3DRMTEXTURE* texture) override;
	D3DRMMAPPING GetGroupMapping(int groupIndex) override;
	D3DRMRENDERQUALITY GetGroupQuality(int groupIndex) override;
	HRESULT GetGroupColor(D3DRMGROUPINDEX index) override;
	HRESULT GetVertices(int groupIndex, int startIndex, int count, D3DRMVERTEX* vertices) override;

private:
	IDirect3DRMTexture* m_groupTexture = nullptr;
};
