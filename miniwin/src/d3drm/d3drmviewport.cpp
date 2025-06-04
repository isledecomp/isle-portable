#include "d3drm_impl.h"
#include "d3drmframe_impl.h"
#include "d3drmrenderer.h"
#include "d3drmviewport_impl.h"
#include "ddraw_impl.h"
#include "miniwin.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_stdinc.h>
#include <cassert>
#include <float.h>
#include <functional>
#include <math.h>

typedef D3DVALUE Matrix3x3[3][3];

Direct3DRMViewportImpl::Direct3DRMViewportImpl(DWORD width, DWORD height, Direct3DRMRenderer* rendere)
	: m_width(width), m_height(height), m_renderer(rendere)
{
}

static void D3DRMMatrixMultiply(D3DRMMATRIX4D out, const D3DRMMATRIX4D a, const D3DRMMATRIX4D b)
{
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			out[i][j] = 0.0f;
			for (int k = 0; k < 4; ++k) {
				out[i][j] += a[i][k] * b[k][j];
			}
		}
	}
}

static void D3DRMMatrixInvertForNormal(Matrix3x3 out, const D3DRMMATRIX4D m)
{
	float a = m[0][0], b = m[0][1], c = m[0][2];
	float d = m[1][0], e = m[1][1], f = m[1][2];
	float g = m[2][0], h = m[2][1], i = m[2][2];

	float det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);

	if (fabs(det) < 1e-6f) {
		memset(out, 0, sizeof(Matrix3x3));
		return;
	}

	float invDet = 1.0f / det;

	out[0][0] = (e * i - f * h) * invDet;
	out[1][0] = (c * h - b * i) * invDet;
	out[2][0] = (b * f - c * e) * invDet;

	out[0][1] = (f * g - d * i) * invDet;
	out[1][1] = (a * i - c * g) * invDet;
	out[2][1] = (c * d - a * f) * invDet;

	out[0][2] = (d * h - e * g) * invDet;
	out[1][2] = (b * g - a * h) * invDet;
	out[2][2] = (a * e - b * d) * invDet;
}

static void D3DRMMatrixInvertOrthogonal(D3DRMMATRIX4D out, const D3DRMMATRIX4D m)
{
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			out[i][j] = m[j][i];
		}
	}

	out[0][3] = out[1][3] = out[2][3] = 0.f;
	out[3][3] = 1.f;

	D3DVECTOR t = {m[3][0], m[3][1], m[3][2]};

	out[3][0] = -(out[0][0] * t.x + out[1][0] * t.y + out[2][0] * t.z);
	out[3][1] = -(out[0][1] * t.x + out[1][1] * t.y + out[2][1] * t.z);
	out[3][2] = -(out[0][2] * t.x + out[1][2] * t.y + out[2][2] * t.z);
}

static void ComputeFrameWorldMatrix(IDirect3DRMFrame* frame, D3DRMMATRIX4D out)
{
	D3DRMMATRIX4D acc = {{1.f, 0.f, 0.f, 0.f}, {0.f, 1.f, 0.f, 0.f}, {0.f, 0.f, 1.f, 0.f}, {0.f, 0.f, 0.f, 1.f}};

	IDirect3DRMFrame* cur = frame;
	while (cur) {
		auto* impl = static_cast<Direct3DRMFrameImpl*>(cur);
		D3DRMMATRIX4D local;
		memcpy(local, impl->m_transform, sizeof(local));

		D3DRMMATRIX4D tmp;
		D3DRMMatrixMultiply(tmp, local, acc);
		memcpy(acc, tmp, sizeof(acc));

		if (cur == impl->m_parent) {
			break;
		}
		cur = impl->m_parent;
	}
	memcpy(out, acc, sizeof(acc));
}

D3DVECTOR ComputeTriangleNormal(const D3DVECTOR& v0, const D3DVECTOR& v1, const D3DVECTOR& v2)
{
	D3DVECTOR u = {v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
	D3DVECTOR v = {v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};
	D3DVECTOR normal = {u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
	float len = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
	if (len > 0.0f) {
		normal.x /= len;
		normal.y /= len;
		normal.z /= len;
	}

	return normal;
}

inline D3DVECTOR TransformPoint(const D3DVECTOR& p, D3DRMMATRIX4D& m)
{
	return {
		p.x * m[0][0] + p.y * m[1][0] + p.z * m[2][0] + m[3][0],
		p.x * m[0][1] + p.y * m[1][1] + p.z * m[2][1] + m[3][1],
		p.x * m[0][2] + p.y * m[1][2] + p.z * m[2][2] + m[3][2]
	};
}

void Direct3DRMViewportImpl::CollectLightsFromFrame(
	IDirect3DRMFrame* frame,
	D3DRMMATRIX4D parentMatrix,
	std::vector<SceneLight>& lights
)
{
	Direct3DRMFrameImpl* frameImpl = static_cast<Direct3DRMFrameImpl*>(frame);
	D3DRMMATRIX4D worldMatrix;
	D3DRMMatrixMultiply(worldMatrix, parentMatrix, frameImpl->m_transform);

	IDirect3DRMLightArray* lightArray = nullptr;
	frame->GetLights(&lightArray);
	DWORD lightCount = lightArray->GetSize();
	for (DWORD li = 0; li < lightCount; ++li) {
		IDirect3DRMLight* light = nullptr;
		lightArray->GetElement(li, &light);
		D3DCOLOR color = light->GetColor();
		D3DRMLIGHTTYPE type = light->GetType();

		SceneLight extracted;
		extracted.color.r = ((color >> 0) & 0xFF) / 255.0f;
		extracted.color.g = ((color >> 8) & 0xFF) / 255.0f;
		extracted.color.b = ((color >> 16) & 0xFF) / 255.0f;
		extracted.color.a = ((color >> 24) & 0xFF) / 255.0f;

		if (type == D3DRMLIGHT_POINT || type == D3DRMLIGHT_SPOT || type == D3DRMLIGHT_PARALLELPOINT) {
			extracted.position.x = worldMatrix[3][0];
			extracted.position.y = worldMatrix[3][1];
			extracted.position.z = worldMatrix[3][2];
			extracted.positional = 1.f;
		}

		if (type == D3DRMLIGHT_DIRECTIONAL || type == D3DRMLIGHT_SPOT) {
			extracted.direction.x = worldMatrix[2][0];
			extracted.direction.y = worldMatrix[2][1];
			extracted.direction.z = worldMatrix[2][2];
			extracted.directional = 1.f;
		}

		lights.push_back(extracted);
		light->Release();
	}
	lightArray->Release();

	IDirect3DRMFrameArray* children = nullptr;
	frame->GetChildren(&children);
	DWORD n = children->GetSize();
	for (DWORD i = 0; i < n; ++i) {
		IDirect3DRMFrame* childFrame = nullptr;
		children->GetElement(i, &childFrame);
		CollectLightsFromFrame(childFrame, worldMatrix, lights);
		childFrame->Release();
	}
	children->Release();
}

struct Plane {
	D3DVECTOR normal;
	float d;
};

void NormalizePlane(Plane& plane)
{
	float len =
		sqrtf(plane.normal.x * plane.normal.x + plane.normal.y * plane.normal.y + plane.normal.z * plane.normal.z);
	if (len > 0.0f) {
		plane.normal.x /= len;
		plane.normal.y /= len;
		plane.normal.z /= len;
		plane.d /= len;
	}
}

Plane frustumPlanes[6];

void ExtractFrustumPlanes(const D3DRMMATRIX4D& m)
{
	// Left
	frustumPlanes[0].normal = {m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0]};
	frustumPlanes[0].d = m[3][3] + m[3][0];

	// Right
	frustumPlanes[1].normal = {m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0]};
	frustumPlanes[1].d = m[3][3] - m[3][0];

	// Bottom
	frustumPlanes[2].normal = {m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1]};
	frustumPlanes[2].d = m[3][3] + m[3][1];

	// Top
	frustumPlanes[3].normal = {m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1]};
	frustumPlanes[3].d = m[3][3] - m[3][1];

	// Near
	frustumPlanes[4].normal = {m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2]};
	frustumPlanes[4].d = m[3][3] + m[3][2];

	// Far
	frustumPlanes[5].normal = {m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2]};
	frustumPlanes[5].d = m[3][3] - m[3][2];

	for (int i = 0; i < 6; ++i) {
		NormalizePlane(frustumPlanes[i]);
	}
}

bool IsBoxInFrustum(const D3DVECTOR corners[8], const Plane planes[6])
{
	for (int i = 0; i < 6; ++i) {
		int out = 0;
		for (int j = 0; j < 8; ++j) {
			const D3DVECTOR& pt = corners[j];
			float dist =
				planes[i].normal.x * pt.x + planes[i].normal.y * pt.y + planes[i].normal.z * pt.z + planes[i].d;
			if (dist < 0.0f) {
				++out;
			}
		}
		if (out == 8) {
			return false; // All corners are outside this plane
		}
	}
	return true;
}

void Direct3DRMViewportImpl::CollectMeshesFromFrame(
	IDirect3DRMFrame* frame,
	D3DRMMATRIX4D parentMatrix,
	std::vector<PositionColorVertex>& verts
)
{
	Direct3DRMFrameImpl* frameImpl = static_cast<Direct3DRMFrameImpl*>(frame);
	D3DRMMATRIX4D localMatrix;
	memcpy(localMatrix, frameImpl->m_transform, sizeof(D3DRMMATRIX4D));

	D3DRMMATRIX4D worldMatrix;
	Matrix3x3 worldMatrixInvert;
	D3DRMMatrixMultiply(worldMatrix, parentMatrix, localMatrix);
	D3DRMMatrixInvertForNormal(worldMatrixInvert, worldMatrix);

	IDirect3DRMVisualArray* visuals = nullptr;
	frame->GetVisuals(&visuals);
	DWORD n = visuals->GetSize();
	for (DWORD i = 0; i < n; ++i) {
		IDirect3DRMVisual* visual = nullptr;
		visuals->GetElement(i, &visual);

		IDirect3DRMFrame* childFrame = nullptr;
		visual->QueryInterface(IID_IDirect3DRMFrame, (void**) &childFrame);
		if (childFrame) {
			CollectMeshesFromFrame(childFrame, worldMatrix, verts);
			childFrame->Release();
			visual->Release();
			continue;
		}

		IDirect3DRMMesh* mesh = nullptr;
		visual->QueryInterface(IID_IDirect3DRMMesh, (void**) &mesh);
		if (mesh) {
			D3DRMBOX box;
			mesh->GetBox(&box);
			D3DVECTOR boxCorners[8] = {
				{box.min.x, box.min.y, box.min.z},
				{box.min.x, box.min.y, box.max.z},
				{box.min.x, box.max.y, box.min.z},
				{box.min.x, box.max.y, box.max.z},
				{box.max.x, box.min.y, box.min.z},
				{box.max.x, box.min.y, box.max.z},
				{box.max.x, box.max.y, box.min.z},
				{box.max.x, box.max.y, box.max.z},
			};
			for (D3DVECTOR& boxCorner : boxCorners) {
				boxCorner = TransformPoint(boxCorner, worldMatrix);
			}
			if (!IsBoxInFrustum(boxCorners, frustumPlanes)) {
				mesh->Release();
				visual->Release();
				continue;
			}

			DWORD groupCount = mesh->GetGroupCount();
			for (DWORD gi = 0; gi < groupCount; ++gi) {
				DWORD vtxCount, faceCount, vpf, dataSize;
				mesh->GetGroup(gi, &vtxCount, &faceCount, &vpf, &dataSize, nullptr);

				std::vector<D3DRMVERTEX> d3dVerts(vtxCount);
				std::vector<DWORD> faces(dataSize);
				mesh->GetVertices(gi, 0, vtxCount, d3dVerts.data());
				mesh->GetGroup(gi, nullptr, nullptr, nullptr, nullptr, faces.data());

				D3DCOLOR color = mesh->GetGroupColor(gi);
				D3DRMRENDERQUALITY quality = mesh->GetGroupQuality(gi);
				IDirect3DRMTexture* texture = nullptr;
				mesh->GetGroupTexture(gi, &texture);
				IDirect3DRMMaterial* material = nullptr;
				mesh->GetGroupMaterial(gi, &material);
				Uint32 texId = NO_TEXTURE_ID;
				if (texture) {
					texId = m_renderer->GetTextureId(texture);
					texture->Release();
				}
				float shininess = 0.0f;
				if (material) {
					shininess = material->GetPower();
					material->Release();
				}

				for (DWORD fi = 0; fi < faceCount; ++fi) {
					D3DVECTOR norm;
					if (quality == D3DRMRENDER_FLAT || quality == D3DRMRENDER_UNLITFLAT) {
						D3DRMVERTEX& v0 = d3dVerts[faces[fi * vpf + 0]];
						D3DRMVERTEX& v1 = d3dVerts[faces[fi * vpf + 1]];
						D3DRMVERTEX& v2 = d3dVerts[faces[fi * vpf + 2]];
						norm = ComputeTriangleNormal(v0.position, v1.position, v2.position);
					}

					for (int idx = 0; idx < vpf; ++idx) {
						D3DRMVERTEX& dv = d3dVerts[faces[fi * vpf + idx]];
						D3DVECTOR pos = dv.position;
						if (quality == D3DRMRENDER_GOURAUD || quality == D3DRMRENDER_PHONG) {
							norm = dv.normal;
						}

						D3DVECTOR worldPos;
						worldPos.x = pos.x * worldMatrix[0][0] + pos.y * worldMatrix[1][0] + pos.z * worldMatrix[2][0] +
									 worldMatrix[3][0];
						worldPos.y = pos.x * worldMatrix[0][1] + pos.y * worldMatrix[1][1] + pos.z * worldMatrix[2][1] +
									 worldMatrix[3][1];
						worldPos.z = pos.x * worldMatrix[0][2] + pos.y * worldMatrix[1][2] + pos.z * worldMatrix[2][2] +
									 worldMatrix[3][2];

						D3DVECTOR viewPos;
						viewPos.x = worldPos.x * m_viewMatrix[0][0] + worldPos.y * m_viewMatrix[1][0] +
									worldPos.z * m_viewMatrix[2][0] + m_viewMatrix[3][0];
						viewPos.y = worldPos.x * m_viewMatrix[0][1] + worldPos.y * m_viewMatrix[1][1] +
									worldPos.z * m_viewMatrix[2][1] + m_viewMatrix[3][1];
						viewPos.z = worldPos.x * m_viewMatrix[0][2] + worldPos.y * m_viewMatrix[1][2] +
									worldPos.z * m_viewMatrix[2][2] + m_viewMatrix[3][2];

						D3DVECTOR viewNorm;
						viewNorm.x = norm.x * worldMatrixInvert[0][0] + norm.y * worldMatrixInvert[1][0] +
									 norm.z * worldMatrixInvert[2][0];
						viewNorm.y = norm.x * worldMatrixInvert[0][1] + norm.y * worldMatrixInvert[1][1] +
									 norm.z * worldMatrixInvert[2][1];
						viewNorm.z = norm.x * worldMatrixInvert[0][2] + norm.y * worldMatrixInvert[1][2] +
									 norm.z * worldMatrixInvert[2][2];

						float len = sqrtf(viewNorm.x * viewNorm.x + viewNorm.y * viewNorm.y + viewNorm.z * viewNorm.z);
						if (len > 0.0f) {
							float invLen = 1.0f / len;
							viewNorm.x *= invLen;
							viewNorm.y *= invLen;
							viewNorm.z *= invLen;
						}

						PositionColorVertex vtx;
						vtx.position = viewPos;
						vtx.normals = viewNorm;
						vtx.colors = {
							static_cast<Uint8>((color >> 16) & 0xFF),
							static_cast<Uint8>((color >> 8) & 0xFF),
							static_cast<Uint8>((color >> 0) & 0xFF),
							static_cast<Uint8>((color >> 24) & 0xFF)
						};
						vtx.shininess = shininess;
						vtx.texId = texId;
						vtx.texCoord = {dv.tu, dv.tv};
						verts.push_back(vtx);
					}
				}
			}
			mesh->Release();
		}
		visual->Release();
	}
	visuals->Release();
}

void Direct3DRMViewportImpl::CollectSceneData()
{
	m_backgroundColor = static_cast<Direct3DRMFrameImpl*>(m_rootFrame)->m_backgroundColor;

	// Compute camera matrix
	D3DRMMATRIX4D cameraWorld, viewProj;
	ComputeFrameWorldMatrix(m_camera, cameraWorld);
	D3DRMMatrixInvertOrthogonal(m_viewMatrix, cameraWorld);
	D3DRMMatrixMultiply(viewProj, m_viewMatrix, m_projectionMatrix);

	D3DRMMATRIX4D identity = {{1.f, 0.f, 0.f, 0.f}, {0.f, 1.f, 0.f, 0.f}, {0.f, 0.f, 1.f, 0.f}, {0.f, 0.f, 0.f, 1.f}};

	std::vector<SceneLight> lights;
	CollectLightsFromFrame(m_rootFrame, identity, lights);
	m_renderer->PushLights(lights.data(), lights.size());

	std::vector<PositionColorVertex> verts;
	ExtractFrustumPlanes(viewProj);
	CollectMeshesFromFrame(m_rootFrame, identity, verts);
	m_renderer->PushVertices(verts.data(), verts.size());
}

HRESULT Direct3DRMViewportImpl::Render(IDirect3DRMFrame* rootFrame)
{
	if (!m_renderer) {
		return DDERR_GENERIC;
	}
	m_rootFrame = rootFrame;
	CollectSceneData();
	return m_renderer->Render();
}

HRESULT Direct3DRMViewportImpl::ForceUpdate(int x, int y, int w, int h)
{
	MINIWIN_NOT_IMPLEMENTED();
	return DD_OK;
}

HRESULT Direct3DRMViewportImpl::Clear()
{
	if (!DDBackBuffer) {
		return DDERR_GENERIC;
	}

	uint8_t r = (m_backgroundColor >> 16) & 0xFF;
	uint8_t g = (m_backgroundColor >> 8) & 0xFF;
	uint8_t b = m_backgroundColor & 0xFF;

	Uint32 color = SDL_MapRGB(SDL_GetPixelFormatDetails(DDBackBuffer->format), nullptr, r, g, b);
	SDL_FillSurfaceRect(DDBackBuffer, NULL, color);
	return DD_OK;
}

HRESULT Direct3DRMViewportImpl::SetCamera(IDirect3DRMFrame* camera)
{
	if (m_camera) {
		m_camera->Release();
	}
	if (camera) {
		camera->AddRef();
	}
	m_camera = camera;
	return DD_OK;
}

HRESULT Direct3DRMViewportImpl::GetCamera(IDirect3DRMFrame** camera)
{
	if (m_camera) {
		m_camera->AddRef();
	}
	*camera = m_camera;
	return DD_OK;
}

HRESULT Direct3DRMViewportImpl::SetProjection(D3DRMPROJECTIONTYPE type)
{
	return DD_OK;
}

D3DRMPROJECTIONTYPE Direct3DRMViewportImpl::GetProjection()
{
	return D3DRMPROJECTIONTYPE::PERSPECTIVE;
}

HRESULT Direct3DRMViewportImpl::SetFront(D3DVALUE z)
{
	m_front = z;
	UpdateProjectionMatrix();
	return DD_OK;
}

D3DVALUE Direct3DRMViewportImpl::GetFront()
{
	return m_front;
}

HRESULT Direct3DRMViewportImpl::SetBack(D3DVALUE z)
{
	m_back = z;
	UpdateProjectionMatrix();
	return DD_OK;
}

D3DVALUE Direct3DRMViewportImpl::GetBack()
{
	return m_back;
}

HRESULT Direct3DRMViewportImpl::SetField(D3DVALUE field)
{
	m_field = field;
	UpdateProjectionMatrix();
	return DD_OK;
}

void Direct3DRMViewportImpl::UpdateProjectionMatrix()
{
	float aspect = (float) m_width / (float) m_height;
	float f = m_front / m_field;
	float depth = m_back - m_front;

	D3DRMMATRIX4D projection = {
		{f, 0, 0, 0},
		{0, f * aspect, 0, 0},
		{0, 0, m_back / depth, 1},
		{0, 0, (-m_front * m_back) / depth, 0},
	};
	memcpy(m_projectionMatrix, projection, sizeof(D3DRMMATRIX4D));

	m_renderer->SetProjection(projection, m_front, m_back);

	D3DRMMATRIX4D inverseProjectionMatrix = {
		{1.0f / f, 0, 0, 0},
		{0, 1.0f / (f * aspect), 0, 0},
		{0, 0, 0, depth / (-m_front * m_back)},
		{0, 0, 1, -(m_back / depth) * depth / (-m_front * m_back)},
	};
	memcpy(m_inverseProjectionMatrix, inverseProjectionMatrix, sizeof(D3DRMMATRIX4D));
}

D3DVALUE Direct3DRMViewportImpl::GetField()
{
	return m_field;
}

DWORD Direct3DRMViewportImpl::GetWidth()
{
	return m_width;
}

DWORD Direct3DRMViewportImpl::GetHeight()
{
	return m_height;
}

HRESULT Direct3DRMViewportImpl::Transform(D3DRMVECTOR4D* screen, D3DVECTOR* world)
{
	D3DRMVECTOR4D worldVec = {world->x, world->y, world->z, 1.0f};

	D3DRMVECTOR4D viewVec;
	viewVec.x = m_viewMatrix[0][0] * worldVec.x + m_viewMatrix[1][0] * worldVec.y + m_viewMatrix[2][0] * worldVec.z +
				m_viewMatrix[3][0] * worldVec.w;
	viewVec.y = m_viewMatrix[0][1] * worldVec.x + m_viewMatrix[1][1] * worldVec.y + m_viewMatrix[2][1] * worldVec.z +
				m_viewMatrix[3][1] * worldVec.w;
	viewVec.z = m_viewMatrix[0][2] * worldVec.x + m_viewMatrix[1][2] * worldVec.y + m_viewMatrix[2][2] * worldVec.z +
				m_viewMatrix[3][2] * worldVec.w;
	viewVec.w = m_viewMatrix[0][3] * worldVec.x + m_viewMatrix[1][3] * worldVec.y + m_viewMatrix[2][3] * worldVec.z +
				m_viewMatrix[3][3] * worldVec.w;

	screen->x = viewVec.x * m_projectionMatrix[0][0] + viewVec.y * m_projectionMatrix[1][0] +
				viewVec.z * m_projectionMatrix[2][0] + viewVec.w * m_projectionMatrix[3][0];
	screen->y = viewVec.x * m_projectionMatrix[0][1] + viewVec.y * m_projectionMatrix[1][1] +
				viewVec.z * m_projectionMatrix[2][1] + viewVec.w * m_projectionMatrix[3][1];
	screen->z = viewVec.x * m_projectionMatrix[0][2] + viewVec.y * m_projectionMatrix[1][2] +
				viewVec.z * m_projectionMatrix[2][2] + viewVec.w * m_projectionMatrix[3][2];
	screen->w = viewVec.x * m_projectionMatrix[0][3] + viewVec.y * m_projectionMatrix[1][3] +
				viewVec.z * m_projectionMatrix[2][3] + viewVec.w * m_projectionMatrix[3][3];

	float invW = 1.0f / screen->w;
	float ndcX = screen->x * invW;
	float ndcY = screen->y * invW;

	screen->x = (ndcX * 0.5f + 0.5f) * m_width;
	screen->y = (1.0f - (ndcY * 0.5f + 0.5f)) * m_height;

	// Undo perspective divide
	screen->x *= screen->z;
	screen->y *= screen->w;

	return DD_OK;
}

HRESULT Direct3DRMViewportImpl::InverseTransform(D3DVECTOR* world, D3DRMVECTOR4D* screen)
{
	// Convert to screen coordinates
	float screenX = screen->x / screen->z;
	float screenY = screen->y / screen->w;

	// Convert screen coordinates to NDC
	float ndcX = screenX / m_width * 2.0f - 1.0f;
	float ndcY = 1.0f - (screenY / m_height) * 2.0f;

	float clipVec[4] = {ndcX * screen->w, ndcY * screen->w, screen->z, screen->w};

	float viewVec[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			viewVec[j] += m_inverseProjectionMatrix[i][j] * clipVec[i];
		}
	}

	D3DRMMATRIX4D inverseViewMatrix;
	D3DRMMatrixInvertOrthogonal(inverseViewMatrix, m_viewMatrix);

	float worldVec[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			worldVec[j] += inverseViewMatrix[i][j] * viewVec[i];
		}
	}

	// Perspective divide
	if (worldVec[3] != 0.0f) {
		world->x = worldVec[0] / worldVec[3];
		world->y = worldVec[1] / worldVec[3];
		world->z = worldVec[2] / worldVec[3];
	}
	else {
		world->x = worldVec[0];
		world->y = worldVec[1];
		world->z = worldVec[2];
	}

	return DD_OK;
}

struct Ray {
	D3DVECTOR origin;
	D3DVECTOR direction;
};

// Ray-box intersection: slab method
bool RayIntersectsBox(const Ray& ray, const D3DRMBOX& box, float& outT)
{
	float tmin = -FLT_MAX;
	float tmax = FLT_MAX;

	for (int i = 0; i < 3; ++i) {
		float origin = (&ray.origin.x)[i];
		float dir = (&ray.direction.x)[i];
		float minB = (&box.min.x)[i];
		float maxB = (&box.max.x)[i];

		if (fabs(dir) < 1e-6f) {
			if (origin < minB || origin > maxB) {
				return false;
			}
		}
		else {
			float invD = 1.0f / dir;
			float t1 = (minB - origin) * invD;
			float t2 = (maxB - origin) * invD;
			if (t1 > t2) {
				std::swap(t1, t2);
			}
			if (t1 > tmin) {
				tmin = t1;
			}
			if (t2 < tmax) {
				tmax = t2;
			}
			if (tmin > tmax) {
				return false;
			}
			if (tmax < 0) {
				return false;
			}
		}
	}

	outT = tmin >= 0 ? tmin : tmax; // closest positive hit
	return true;
}

inline float DotProduct(const D3DVECTOR& a, const D3DVECTOR& b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Convert screen (x,y) in viewport to picking ray in world space
Ray BuildPickingRay(
	float x,
	float y,
	int width,
	int height,
	IDirect3DRMFrame* camera,
	float front,
	float back,
	float field,
	float aspect
)
{
	float nx = (2.0f * x) / width - 1.0f;
	float ny = 1.0f - (2.0f * y) / height;

	float f = front / field;

	// Ray in view space at near plane:
	D3DVECTOR rayDirView = {nx / f, ny / (f * aspect), 1.0f};

	// Normalize ray direction
	float len = sqrt(DotProduct(rayDirView, rayDirView));
	rayDirView.x /= len;
	rayDirView.y /= len;
	rayDirView.z /= len;

	// Compute camera world matrix and invert it to get view->world
	D3DRMMATRIX4D cameraWorld;
	ComputeFrameWorldMatrix(camera, cameraWorld);

	// Transform ray origin (camera position) and direction to world space
	D3DVECTOR rayOriginWorld = {cameraWorld[3][0], cameraWorld[3][1], cameraWorld[3][2]};
	// Multiply direction by rotation part of matrix only (3x3 upper-left)
	D3DVECTOR rayDirWorld = {
		rayDirView.x * cameraWorld[0][0] + rayDirView.y * cameraWorld[1][0] + rayDirView.z * cameraWorld[2][0],
		rayDirView.x * cameraWorld[0][1] + rayDirView.y * cameraWorld[1][1] + rayDirView.z * cameraWorld[2][1],
		rayDirView.x * cameraWorld[0][2] + rayDirView.y * cameraWorld[1][2] + rayDirView.z * cameraWorld[2][2]
	};

	len = sqrt(rayDirWorld.x * rayDirWorld.x + rayDirWorld.y * rayDirWorld.y + rayDirWorld.z * rayDirWorld.z);
	rayDirWorld.x /= len;
	rayDirWorld.y /= len;
	rayDirWorld.z /= len;

	return Ray{rayOriginWorld, rayDirWorld};
}

inline D3DVECTOR CrossProduct(const D3DVECTOR& a, const D3DVECTOR& b)
{
	return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

bool RayIntersectsTriangle(
	const Ray& ray,
	const D3DVECTOR& v0,
	const D3DVECTOR& v1,
	const D3DVECTOR& v2,
	float& outDist
)
{
	const float EPSILON = 1e-6f;
	D3DVECTOR edge1 = {v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
	D3DVECTOR edge2 = {v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};

	D3DVECTOR h = CrossProduct(ray.direction, edge2);
	float a = DotProduct(edge1, h);
	if (fabs(a) < EPSILON) {
		return false;
	}

	float f = 1.0f / a;
	D3DVECTOR s = {ray.origin.x - v0.x, ray.origin.y - v0.y, ray.origin.z - v0.z};
	float u = f * DotProduct(s, h);
	if (u < 0.0f || u > 1.0f) {
		return false;
	}

	D3DVECTOR q = CrossProduct(s, edge1);
	float v = f * DotProduct(ray.direction, q);
	if (v < 0.0f || u + v > 1.0f) {
		return false;
	}

	float t = f * DotProduct(edge2, q);
	if (t > EPSILON) {
		outDist = t;
		return true;
	}
	return false;
}

bool RayIntersectsMeshTriangles(
	const Ray& ray,
	IDirect3DRMMesh* mesh,
	const D3DRMMATRIX4D& worldMatrix,
	float& outDistance
)
{
	DWORD groupCount = mesh->GetGroupCount();
	for (DWORD g = 0; g < groupCount; ++g) {
		DWORD vtxCount = 0, faceCount = 0, vpf = 0, dataSize = 0;
		mesh->GetGroup(g, &vtxCount, &faceCount, &vpf, &dataSize, nullptr);

		std::vector<D3DRMVERTEX> vertices(vtxCount);
		mesh->GetVertices(g, 0, vtxCount, vertices.data());
		std::vector<DWORD> faces(faceCount * vpf);
		mesh->GetGroup(g, nullptr, nullptr, nullptr, nullptr, faces.data());

		// Iterate over each face and do ray-triangle tests
		for (DWORD fi = 0; fi < faceCount; ++fi) {
			DWORD i0 = faces[fi * vpf + 0];
			DWORD i1 = faces[fi * vpf + 1];
			DWORD i2 = faces[fi * vpf + 2];

			if (i0 >= vtxCount || i1 >= vtxCount || i2 >= vtxCount) {
				continue;
			}

			// Transform vertices to world space
			D3DVECTOR tri[3];
			for (int j = 0; j < 3; ++j) {
				const D3DVECTOR& v = vertices[(j == 0 ? i0 : (j == 1 ? i1 : i2))].position;
				tri[j].x =
					v.x * worldMatrix[0][0] + v.y * worldMatrix[1][0] + v.z * worldMatrix[2][0] + worldMatrix[3][0];
				tri[j].y =
					v.x * worldMatrix[0][1] + v.y * worldMatrix[1][1] + v.z * worldMatrix[2][1] + worldMatrix[3][1];
				tri[j].z =
					v.x * worldMatrix[0][2] + v.y * worldMatrix[1][2] + v.z * worldMatrix[2][2] + worldMatrix[3][2];
			}

			float dist;
			if (RayIntersectsTriangle(ray, tri[0], tri[1], tri[2], dist)) {
				if (dist < outDistance) {
					outDistance = dist;
				}
				return true;
			}
		}
	}
	return false;
}

HRESULT Direct3DRMViewportImpl::Pick(float x, float y, LPDIRECT3DRMPICKEDARRAY* pickedArray)
{
	if (!m_rootFrame) {
		return DDERR_GENERIC;
	}

	std::vector<PickRecord> hits;

	Ray pickRay = BuildPickingRay(
		x,
		y,
		m_width,
		m_height,
		m_camera,
		m_front,
		m_back,
		m_field,
		(float) m_width / (float) m_height
	);

	std::function<void(IDirect3DRMFrame*, std::vector<IDirect3DRMFrame*>&)> recurse;
	recurse = [&](IDirect3DRMFrame* frame, std::vector<IDirect3DRMFrame*>& path) {
		Direct3DRMFrameImpl* frameImpl = static_cast<Direct3DRMFrameImpl*>(frame);
		path.push_back(frame); // Push current frame

		IDirect3DRMVisualArray* visuals = nullptr;
		frame->GetVisuals(&visuals);
		DWORD count = visuals->GetSize();
		for (DWORD i = 0; i < count; ++i) {
			IDirect3DRMVisual* visual = nullptr;
			visuals->GetElement(i, &visual);

			IDirect3DRMFrame* subFrame = nullptr;
			visual->QueryInterface(IID_IDirect3DRMFrame, (void**) &subFrame);
			if (subFrame) {
				recurse(subFrame, path);
				subFrame->Release();
				visual->Release();
				continue;
			}

			IDirect3DRMMesh* mesh = nullptr;
			visual->QueryInterface(IID_IDirect3DRMMesh, (void**) &mesh);
			if (mesh) {
				D3DRMBOX box;
				mesh->GetBox(&box);
				// Transform box corners to world space
				D3DRMMATRIX4D worldMatrix;
				ComputeFrameWorldMatrix(frame, worldMatrix);

				// Transform box min and max points
				// Because axis-aligned box can become oriented box after transform,
				// but we simplify by transforming all 8 corners and computing new AABB

				D3DVECTOR corners[8] = {
					{box.min.x, box.min.y, box.min.z},
					{box.min.x, box.min.y, box.max.z},
					{box.min.x, box.max.y, box.min.z},
					{box.min.x, box.max.y, box.max.z},
					{box.max.x, box.min.y, box.min.z},
					{box.max.x, box.min.y, box.max.z},
					{box.max.x, box.max.y, box.min.z},
					{box.max.x, box.max.y, box.max.z},
				};

				D3DRMBOX worldBox;
				{
					float x = corners[0].x * worldMatrix[0][0] + corners[0].y * worldMatrix[1][0] +
							  corners[0].z * worldMatrix[2][0] + worldMatrix[3][0];
					float y = corners[0].x * worldMatrix[0][1] + corners[0].y * worldMatrix[1][1] +
							  corners[0].z * worldMatrix[2][1] + worldMatrix[3][1];
					float z = corners[0].x * worldMatrix[0][2] + corners[0].y * worldMatrix[1][2] +
							  corners[0].z * worldMatrix[2][2] + worldMatrix[3][2];
					worldBox.min = {x, y, z};
					worldBox.max = {x, y, z};
				}

				for (int c = 1; c < 8; ++c) {
					float x = corners[c].x * worldMatrix[0][0] + corners[c].y * worldMatrix[1][0] +
							  corners[c].z * worldMatrix[2][0] + worldMatrix[3][0];
					float y = corners[c].x * worldMatrix[0][1] + corners[c].y * worldMatrix[1][1] +
							  corners[c].z * worldMatrix[2][1] + worldMatrix[3][1];
					float z = corners[c].x * worldMatrix[0][2] + corners[c].y * worldMatrix[1][2] +
							  corners[c].z * worldMatrix[2][2] + worldMatrix[3][2];

					if (x < worldBox.min.x) {
						worldBox.min.x = x;
					}
					if (y < worldBox.min.y) {
						worldBox.min.y = y;
					}
					if (z < worldBox.min.z) {
						worldBox.min.z = z;
					}
					if (x > worldBox.max.x) {
						worldBox.max.x = x;
					}
					if (y > worldBox.max.y) {
						worldBox.max.y = y;
					}
					if (z > worldBox.max.z) {
						worldBox.max.z = z;
					}
				}

				float distance = 0.0f;
				if (RayIntersectsBox(pickRay, worldBox, distance) &&
					RayIntersectsMeshTriangles(pickRay, mesh, worldMatrix, distance)) {
					auto* arr = new Direct3DRMFrameArrayImpl();
					for (IDirect3DRMFrame* f : path) {
						arr->AddElement(f);
					}

					PickRecord rec;
					rec.visual = visual;
					rec.frameArray = arr;
					rec.desc.dist = distance;
					hits.push_back(rec);
				}
				mesh->Release();
			}
			visual->Release();
		}
		visuals->Release();
		path.pop_back(); // Pop after recursion
	};

	std::vector<IDirect3DRMFrame*> framePath;
	recurse(m_rootFrame, framePath);

	std::sort(hits.begin(), hits.end(), [](const PickRecord& a, const PickRecord& b) {
		return a.desc.dist < b.desc.dist;
	});

	*pickedArray = new Direct3DRMPickedArrayImpl(hits.data(), hits.size());

	return D3DRM_OK;
}

void Direct3DRMViewportImpl::CloseDevice()
{
	m_renderer = nullptr;
}
