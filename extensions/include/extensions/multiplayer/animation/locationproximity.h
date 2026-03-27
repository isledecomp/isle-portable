#pragma once

#include <cstdint>
#include <vector>

namespace Multiplayer::Animation
{

static constexpr float NPC_ANIM_PROXIMITY = 15.0f;

class LocationProximity {
public:
	LocationProximity();

	// Returns true if location set changed since last call
	bool Update(float p_x, float p_z);

	// All locations within radius (sorted by index for stable comparison)
	const std::vector<int16_t>& GetLocations() const { return m_locations; }
	bool IsAtLocation(int16_t p_location) const;

	float GetRadius() const { return m_radius; }
	void Reset();

	// Static version returning all locations within radius (sorted by index)
	static std::vector<int16_t> ComputeAll(float p_x, float p_z, float p_radius);

private:
	float m_radius;
	std::vector<int16_t> m_locations;
};

} // namespace Multiplayer::Animation
