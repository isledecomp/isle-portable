#include "extensions/multiplayer/namebubblerenderer.h"

#include "3dmanager/lego3dmanager.h"
#include "legovideomanager.h"
#include "misc.h"
#include "roi/legoroi.h"
#include "tgl/tgl.h"

#include <SDL3/SDL_stdinc.h>
#include <vec.h>

using namespace Multiplayer;

// 5x5 bitmap font for A-Z (each row is a byte with bits 4..0 representing columns)
// clang-format off
static const uint8_t g_letterFont[26][5] = {
	{0x0E, 0x11, 0x1F, 0x11, 0x11}, // A
	{0x1E, 0x11, 0x1E, 0x11, 0x1E}, // B
	{0x0F, 0x10, 0x10, 0x10, 0x0F}, // C
	{0x1E, 0x11, 0x11, 0x11, 0x1E}, // D
	{0x1F, 0x10, 0x1E, 0x10, 0x1F}, // E
	{0x1F, 0x10, 0x1E, 0x10, 0x10}, // F
	{0x0F, 0x10, 0x13, 0x11, 0x0F}, // G
	{0x11, 0x11, 0x1F, 0x11, 0x11}, // H
	{0x0E, 0x04, 0x04, 0x04, 0x0E}, // I
	{0x01, 0x01, 0x01, 0x11, 0x0E}, // J
	{0x11, 0x12, 0x1C, 0x12, 0x11}, // K
	{0x10, 0x10, 0x10, 0x10, 0x1F}, // L
	{0x11, 0x1B, 0x15, 0x11, 0x11}, // M
	{0x11, 0x19, 0x15, 0x13, 0x11}, // N
	{0x0E, 0x11, 0x11, 0x11, 0x0E}, // O
	{0x1E, 0x11, 0x1E, 0x10, 0x10}, // P
	{0x0E, 0x11, 0x15, 0x12, 0x0D}, // Q
	{0x1E, 0x11, 0x1E, 0x12, 0x11}, // R
	{0x0F, 0x10, 0x0E, 0x01, 0x1E}, // S
	{0x1F, 0x04, 0x04, 0x04, 0x04}, // T
	{0x11, 0x11, 0x11, 0x11, 0x0E}, // U
	{0x11, 0x11, 0x11, 0x0A, 0x04}, // V
	{0x11, 0x11, 0x15, 0x1B, 0x11}, // W
	{0x11, 0x0A, 0x04, 0x0A, 0x11}, // X
	{0x11, 0x0A, 0x04, 0x04, 0x04}, // Y
	{0x1F, 0x02, 0x04, 0x08, 0x1F}, // Z
};
// clang-format on

// Texture dimensions (must be power of 2)
static const int TEX_WIDTH = 64;
static const int TEX_HEIGHT = 16;

// Palette indices
static const uint8_t PAL_TRANSPARENT = 0;
static const uint8_t PAL_BLACK = 1;
static const uint8_t PAL_WHITE = 2;

// Billboard world-space size
static const float BUBBLE_WIDTH = 1.2f;
static const float BUBBLE_HEIGHT = 0.3f;

// Vertical offset above ROI bounding sphere
static const float BUBBLE_Y_OFFSET = 0.15f;

NameBubbleRenderer::NameBubbleRenderer()
	: m_group(nullptr), m_meshBuilder(nullptr), m_mesh(nullptr), m_texture(nullptr), m_texelData(nullptr),
	  m_visible(true)
{
}

NameBubbleRenderer::~NameBubbleRenderer()
{
	Destroy();
}

void NameBubbleRenderer::GenerateTexture(const char* p_name)
{
	m_texelData = new uint8_t[TEX_WIDTH * TEX_HEIGHT];
	SDL_memset(m_texelData, PAL_TRANSPARENT, TEX_WIDTH * TEX_HEIGHT);

	int nameLen = (int) SDL_strlen(p_name);
	if (nameLen <= 0) {
		return;
	}

	// Each letter is 5px wide + 1px spacing; 3px horizontal and 2px vertical padding
	int bubbleW = nameLen * 6 - 1 + 6;
	int bubbleH = 9;
	int bx = SDL_max((TEX_WIDTH - bubbleW) / 2, 0);
	int by = SDL_max((TEX_HEIGHT - bubbleH) / 2, 0);

	// Draw white bubble background with rounded corners
	for (int y = by; y < by + bubbleH && y < TEX_HEIGHT; y++) {
		for (int x = bx; x < bx + bubbleW && x < TEX_WIDTH; x++) {
			int lx = x - bx;
			int ly = y - by;
			if ((lx == 0 || lx == bubbleW - 1) && (ly == 0 || ly == bubbleH - 1)) {
				continue;
			}
			m_texelData[y * TEX_WIDTH + x] = PAL_WHITE;
		}
	}

	// Draw black border (top/bottom edges, then left/right edges)
	for (int x = bx + 1; x < bx + bubbleW - 1 && x < TEX_WIDTH; x++) {
		m_texelData[by * TEX_WIDTH + x] = PAL_BLACK;
		m_texelData[(by + bubbleH - 1) * TEX_WIDTH + x] = PAL_BLACK;
	}
	for (int y = by + 1; y < by + bubbleH - 1 && y < TEX_HEIGHT; y++) {
		m_texelData[y * TEX_WIDTH + bx] = PAL_BLACK;
		m_texelData[y * TEX_WIDTH + bx + bubbleW - 1] = PAL_BLACK;
	}

	// Draw text (black on white bubble)
	int textX = bx + 3;
	int textY = by + 2;

	for (int i = 0; i < nameLen; i++) {
		char ch = SDL_toupper(p_name[i]);
		if (ch < 'A' || ch > 'Z') {
			continue;
		}

		for (int row = 0; row < 5; row++) {
			uint8_t bits = g_letterFont[ch - 'A'][row];
			for (int col = 0; col < 5; col++) {
				if (bits & (1 << (4 - col))) {
					int px = textX + i * 6 + col;
					int py = textY + row;
					if (px < TEX_WIDTH && py < TEX_HEIGHT) {
						m_texelData[py * TEX_WIDTH + px] = PAL_BLACK;
					}
				}
			}
		}
	}
}

void NameBubbleRenderer::CreateQuadMesh()
{
	Tgl::Renderer* renderer = VideoManager()->GetRenderer();
	if (!renderer) {
		return;
	}

	m_meshBuilder = renderer->CreateMeshBuilder();
	if (!m_meshBuilder) {
		return;
	}

	float halfW = BUBBLE_WIDTH * 0.5f;
	float halfH = BUBBLE_HEIGHT * 0.5f;

	// Vertex order chosen so that triangles (0,1,2) and (0,2,3) have CW winding
	// when viewed from +Z, matching the renderer's glFrontFace(GL_CW) setting.
	float positions[4][3] = {
		{-halfW, -halfH, 0.0f}, // 0: bottom-left
		{-halfW, halfH, 0.0f},  // 1: top-left
		{halfW, halfH, 0.0f},   // 2: top-right
		{halfW, -halfH, 0.0f}   // 3: bottom-right
	};

	float normals[4][3] = {{0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}};

	float texCoords[4][2] = {
		{0.0f, 1.0f}, // 0: bottom-left of texture
		{0.0f, 0.0f}, // 1: top-left
		{1.0f, 0.0f}, // 2: top-right
		{1.0f, 1.0f}  // 3: bottom-right
	};

	// Tgl::CreateMesh expects packed face indices where each uint32 encodes:
	//   low 16 bits  = position vertex index
	//   high 16 bits = normal vertex index | 0x8000 (bit 15 = "packed vertex" flag)
	// Without the 0x8000 flag, the entry is a simple reference to an already-created
	// vertex (no new vertex is allocated).  Each packed entry creates a new vertex,
	// so shared vertices (0 and 2, used in both triangles) must use simple refs in
	// the second triangle to stay within the p_numVertices allocation.
	unsigned int faceIndices[2][3] = {
		{0x80000000, 0x80010001, 0x80020002}, // create vertices 0, 1, 2
		{0x00000000, 0x00000002, 0x80030003}  // reuse 0, reuse 2, create vertex 3
	};

	unsigned int texIndices[2][3] = {
		{0, 1, 2},
		{0, 0, 3} // only index 5 (value 3) is read; indices 3-4 are simple refs
	};

	m_mesh = m_meshBuilder->CreateMesh(2, 4, positions, normals, texCoords, faceIndices, texIndices, Tgl::Flat);
}

void NameBubbleRenderer::Create(const char* p_name)
{
	if (m_group || !p_name || p_name[0] == '\0') {
		return;
	}

	Tgl::Renderer* renderer = VideoManager()->GetRenderer();
	if (!renderer) {
		return;
	}

	// Generate the name texture
	GenerateTexture(p_name);

	// Create Tgl texture from pixel data
	Tgl::PaletteEntry palette[3];
	palette[PAL_TRANSPARENT] = {255, 255, 255};
	palette[PAL_BLACK] = {0, 0, 0};
	palette[PAL_WHITE] = {255, 255, 255};

	m_texture = renderer->CreateTexture(TEX_WIDTH, TEX_HEIGHT, 8, m_texelData, TRUE, 3, palette);
	if (!m_texture) {
		Destroy();
		return;
	}

	// Create the quad mesh
	CreateQuadMesh();
	if (!m_mesh) {
		Destroy();
		return;
	}

	// Apply texture to mesh
	m_mesh->SetTexture(m_texture);
	m_mesh->SetShadingModel(Tgl::Flat);
	// Set alpha < 1.0 so the renderer treats this as transparent (deferred draw
	// with blending enabled). The actual per-pixel alpha comes from the texture.
	m_mesh->SetColor(1.0f, 1.0f, 1.0f, 254.0f / 255.0f);

	// Create a group (D3DRM frame) to hold the billboard
	Tgl::Group* scene = VideoManager()->Get3DManager()->GetScene();
	m_group = renderer->CreateGroup(scene);
	if (!m_group) {
		Destroy();
		return;
	}

	m_group->Add(m_meshBuilder);
}

void NameBubbleRenderer::Destroy()
{
	if (m_group) {
		if (m_visible) {
			Tgl::Group* scene = VideoManager()->Get3DManager()->GetScene();
			if (scene) {
				scene->Remove(m_group);
			}
		}
		delete m_group;
		m_group = nullptr;
	}

	if (m_meshBuilder) {
		delete m_meshBuilder;
		m_meshBuilder = nullptr;
		m_mesh = nullptr; // owned by meshBuilder
	}

	if (m_texture) {
		delete m_texture;
		m_texture = nullptr;
	}

	if (m_texelData) {
		delete[] m_texelData;
		m_texelData = nullptr;
	}
}

void NameBubbleRenderer::SetVisible(bool p_visible)
{
	if (m_visible == p_visible || !m_group) {
		return;
	}

	m_visible = p_visible;

	Tgl::Group* scene = VideoManager()->Get3DManager()->GetScene();
	if (!scene) {
		return;
	}

	if (p_visible) {
		scene->Add(m_group);
	}
	else {
		scene->Remove(m_group);
	}
}

void NameBubbleRenderer::Update(LegoROI* p_roi)
{
	if (!m_group || !p_roi || !m_visible) {
		return;
	}

	LegoROI* viewROI = VideoManager()->GetViewROI();
	if (!viewROI) {
		return;
	}

	// Billboard normal = camera's backward-z direction (faces toward camera)
	const float* normal = viewROI->GetWorldDirection();
	const float* camUp = viewROI->GetWorldUp();

	// Build billboard basis vectors
	float right[3], up[3];
	VXV3(right, camUp, normal);
	float rLen = SDL_sqrtf(NORMSQRD3(right));
	if (rLen > 0.0001f) {
		VDS3(right, right, rLen);
	}
	VXV3(up, normal, right);

	// Position above the player's bounding sphere
	const BoundingSphere& sphere = p_roi->GetWorldBoundingSphere();
	float pos[3];
	SET3(pos, sphere.Center());
	pos[1] += sphere.Radius() + BUBBLE_Y_OFFSET;

	// Build transformation: rows are right, up, normal, position
	Tgl::FloatMatrix4 mat = {};
	SET3(mat[0], right);
	SET3(mat[1], up);
	SET3(mat[2], normal);
	SET3(mat[3], pos);
	mat[3][3] = 1.0f;

	m_group->SetTransformation(mat);
}
