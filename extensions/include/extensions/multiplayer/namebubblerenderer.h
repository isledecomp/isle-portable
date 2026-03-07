#pragma once

#include <cstdint>

namespace Tgl
{
class Group;
class MeshBuilder;
class Mesh;
class Texture;
} // namespace Tgl

class LegoROI;

namespace Multiplayer
{

class NameBubbleRenderer {
public:
	NameBubbleRenderer();
	~NameBubbleRenderer();

	// Create the 3D billboard with the given name text.
	// Must be called after the player's ROI is spawned.
	void Create(const char* p_name);

	// Remove from scene and release all resources.
	void Destroy();

	// Update billboard position (above p_roi) and orientation (face camera).
	void Update(LegoROI* p_roi);

	// Show or hide the billboard.
	void SetVisible(bool p_visible);

	bool IsCreated() const { return m_group != nullptr; }

private:
	void GenerateTexture(const char* p_name);
	void CreateQuadMesh();

	Tgl::Group* m_group;
	Tgl::MeshBuilder* m_meshBuilder;
	Tgl::Mesh* m_mesh;
	Tgl::Texture* m_texture;
	uint8_t* m_texelData;
	bool m_visible;
};

} // namespace Multiplayer
