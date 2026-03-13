#pragma once

#include "mxtypes.h"
#include "mxgeometry/mxmatrix.h"
#include "realtime/vector.h"
#include "roi/legoroi.h"

#include <map>
#include <string>

class LegoAnim;

namespace Extensions
{
namespace Common
{

namespace AnimUtils
{

// Cached ROI map entry for an animation
struct AnimCache {
	LegoAnim* anim;
	LegoROI** roiMap;
	MxU32 roiMapSize;

	AnimCache() : anim(nullptr), roiMap(nullptr), roiMapSize(0) {}
	~AnimCache()
	{
		if (roiMap) {
			delete[] roiMap;
		}
	}

	AnimCache(const AnimCache&) = delete;
	AnimCache& operator=(const AnimCache&) = delete;
	AnimCache(AnimCache&& p_other) noexcept
		: anim(p_other.anim), roiMap(p_other.roiMap), roiMapSize(p_other.roiMapSize)
	{
		p_other.roiMap = nullptr;
		p_other.roiMapSize = 0;
		p_other.anim = nullptr;
	}
	AnimCache& operator=(AnimCache&& p_other) noexcept
	{
		if (this != &p_other) {
			if (roiMap) {
				delete[] roiMap;
			}
			anim = p_other.anim;
			roiMap = p_other.roiMap;
			roiMapSize = p_other.roiMapSize;
			p_other.roiMap = nullptr;
			p_other.roiMapSize = 0;
			p_other.anim = nullptr;
		}
		return *this;
	}
};

void BuildROIMap(
	LegoAnim* p_anim,
	LegoROI* p_rootROI,
	LegoROI* p_extraROI,
	LegoROI**& p_roiMap,
	MxU32& p_roiMapSize
);

AnimCache* GetOrBuildAnimCache(
	std::map<std::string, AnimCache>& p_cacheMap,
	LegoROI* p_roi,
	const char* p_animName
);

inline void EnsureROIMapVisibility(LegoROI** p_roiMap, MxU32 p_roiMapSize)
{
	for (MxU32 i = 1; i < p_roiMapSize; i++) {
		if (p_roiMap[i] != nullptr) {
			p_roiMap[i]->SetVisibility(TRUE);
		}
	}
}

// Flip a matrix from forward-z to backward-z (or vice versa) in place.
inline void FlipMatrixDirection(MxMatrix& p_mat)
{
	Vector3 right(p_mat[0]);
	Vector3 up(p_mat[1]);
	Vector3 direction(p_mat[2]);
	direction *= -1.0f;
	right.EqualsCross(up, direction);
}

} // namespace AnimUtils

} // namespace Common
} // namespace Extensions
