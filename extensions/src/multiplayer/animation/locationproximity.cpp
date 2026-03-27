#include "extensions/multiplayer/animation/locationproximity.h"

#include "decomp.h"
#include "legolocations.h"

#include <algorithm>
#include <cmath>

using namespace Multiplayer::Animation;

static const float DEFAULT_RADIUS = 5.0f;

// Location 0 is the camera origin, and the last location is overhead — skip both
static const int FIRST_VALID_LOCATION = 1;
static const int LAST_VALID_LOCATION = sizeOfArray(g_locations) - 2;

LocationProximity::LocationProximity() : m_radius(DEFAULT_RADIUS)
{
}

bool LocationProximity::Update(float p_x, float p_z)
{
	std::vector<int16_t> prev = m_locations;
	m_locations = ComputeAll(p_x, p_z, m_radius);
	return m_locations != prev;
}

bool LocationProximity::IsAtLocation(int16_t p_location) const
{
	return std::find(m_locations.begin(), m_locations.end(), p_location) != m_locations.end();
}

void LocationProximity::Reset()
{
	m_locations.clear();
}

std::vector<int16_t> LocationProximity::ComputeAll(float p_x, float p_z, float p_radius)
{
	std::vector<int16_t> result;

	for (int i = FIRST_VALID_LOCATION; i <= LAST_VALID_LOCATION; i++) {
		float dx = p_x - g_locations[i].m_position[0];
		float dz = p_z - g_locations[i].m_position[2];
		float dist = std::sqrt(dx * dx + dz * dz);

		if (dist < p_radius) {
			result.push_back(static_cast<int16_t>(i));
		}
	}

	// Sorted by index (iteration order is already ascending), which gives stable comparison
	return result;
}
