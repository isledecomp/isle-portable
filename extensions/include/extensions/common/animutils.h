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
// Used for participant ROIs whose real names (e.g. "tp_display") differ
// from the animation tree node names (e.g. "pepper").
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

void CollectUnmatchedNodes(LegoAnim* p_anim, LegoROI* p_rootROI, std::vector<std::string>& p_unmatchedNames);

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

// Deep-clone a ROI hierarchy, sharing LOD geometry via refcount.
// Each clone gets its own transform, safe for concurrent animation playback.
LegoROI* DeepCloneROI(LegoROI* p_source, const char* p_name);

// Strip trailing digits and underscores from a name to get the LOD base name.
// Mirrors the digit-trimming in LegoAnimPresenter::CreateManagedActors/CreateSceneROIs.
std::string TrimLODSuffix(const std::string& p_name);

// Maps animation tree node names to actual LOD names when they differ.
const char* ResolvePropLODName(const char* p_nodeName);

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
