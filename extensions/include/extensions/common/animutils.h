#pragma once

#include "mxgeometry/mxmatrix.h"
#include "mxtypes.h"
#include "realtime/vector.h"
#include "roi/legoroi.h"

#include <map>
#include <string>
#include <vector>

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
	AnimCache(AnimCache&& p_other) noexcept : anim(p_other.anim), roiMap(p_other.roiMap), roiMapSize(p_other.roiMapSize)
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

// Maps an animation character name to an ROI without renaming the ROI.
// Used for participant ROIs whose real names differ from the animation
// tree node names.
struct ROIAlias {
	const char* animName; // name in animation tree (lowercased)
	LegoROI* roi;         // actual ROI to use
};

void BuildROIMap(
	LegoAnim* p_anim,
	LegoROI* p_rootROI,
	LegoROI** p_extraROIs,
	int p_extraROICount,
	LegoROI**& p_roiMap,
	MxU32& p_roiMapSize,
	const ROIAlias* p_aliases = nullptr,
	int p_aliasCount = 0
);

AnimCache* GetOrBuildAnimCache(std::map<std::string, AnimCache>& p_cacheMap, LegoROI* p_roi, const char* p_animName);

inline void EnsureROIMapVisibility(LegoROI** p_roiMap, MxU32 p_roiMapSize)
{
	for (MxU32 i = 1; i < p_roiMapSize; i++) {
		if (p_roiMap[i] != nullptr) {
			p_roiMap[i]->SetVisibility(TRUE);
		}
	}
}

// Apply animation transformation to all root children of an animation tree.
void ApplyTree(LegoAnim* p_anim, MxMatrix& p_transform, LegoTime p_time, LegoROI** p_roiMap);

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
