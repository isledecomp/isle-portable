#pragma once

#include <cstdint>

namespace Multiplayer::Animation
{

static constexpr float NPC_ANIM_PROXIMITY = 15.0f;

class LocationProximity {
public:
	LocationProximity();

	// Returns true if nearest location changed since last call
	bool Update(float p_x, float p_z);

	int16_t GetNearestLocation() const { return m_nearestLocation; }
	float GetNearestDistance() const { return m_nearestDistance; }

	void SetRadius(float p_radius) { m_radius = p_radius; }
	float GetRadius() const { return m_radius; }
	void Reset();

	// Static version for computing any position's nearest location
	static int16_t ComputeNearest(float p_x, float p_z, float p_radius);

private:
	int16_t m_nearestLocation;
	float m_nearestDistance;
	float m_radius;
};

} // namespace Multiplayer::Animation
