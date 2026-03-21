#include "extensions/multiplayer/animation/locationproximity.h"

#include "decomp.h"
#include "legolocations.h"

#include <cmath>

using namespace Multiplayer::Animation;

static const float DEFAULT_RADIUS = NPC_ANIM_PROXIMITY;

// Location 0 is the camera origin, and the last location is overhead — skip both
static const int FIRST_VALID_LOCATION = 1;
static const int LAST_VALID_LOCATION = sizeOfArray(g_locations) - 2;

LocationProximity::LocationProximity() : m_nearestLocation(-1), m_nearestDistance(0.0f), m_radius(DEFAULT_RADIUS)
{
}

bool LocationProximity::Update(float p_x, float p_z)
{
	int16_t prev = m_nearestLocation;
	m_nearestLocation = ComputeNearest(p_x, p_z, m_radius);

	if (m_nearestLocation >= 0) {
		float dx = p_x - g_locations[m_nearestLocation].m_position[0];
		float dz = p_z - g_locations[m_nearestLocation].m_position[2];
		m_nearestDistance = std::sqrt(dx * dx + dz * dz);
	}
	else {
		m_nearestDistance = 0.0f;
	}

	return m_nearestLocation != prev;
}

void LocationProximity::Reset()
{
	m_nearestLocation = -1;
	m_nearestDistance = 0.0f;
}

int16_t LocationProximity::ComputeNearest(float p_x, float p_z, float p_radius)
{
	float bestDist = p_radius;
	int16_t bestLocation = -1;

	for (int i = FIRST_VALID_LOCATION; i <= LAST_VALID_LOCATION; i++) {
		float dx = p_x - g_locations[i].m_position[0];
		float dz = p_z - g_locations[i].m_position[2];
		float dist = std::sqrt(dx * dx + dz * dz);

		if (dist < bestDist) {
			bestDist = dist;
			bestLocation = static_cast<int16_t>(i);
		}
	}

	return bestLocation;
}
