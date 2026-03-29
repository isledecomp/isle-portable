#pragma once

#include "legogamestate.h"

namespace Extensions
{
namespace Common
{

// Overlay areas within the Isle world (e_act1) that use fixed camera angles
// and have no free-roaming player movement. The player character should not
// be visible in these areas.
inline bool IsRestrictedArea(LegoGameState::Area p_area)
{
	switch (p_area) {
	case LegoGameState::e_elevride:
	case LegoGameState::e_elevride2:
	case LegoGameState::e_elevopen:
	case LegoGameState::e_seaview:
	case LegoGameState::e_observe:
	case LegoGameState::e_elevdown:
	case LegoGameState::e_garadoor:
	case LegoGameState::e_polidoor:
		return true;
	default:
		return false;
	}
}

} // namespace Common
} // namespace Extensions
