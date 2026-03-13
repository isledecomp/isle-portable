#include "extensions/common/animutils.h"

#include "anim/legoanim.h"
#include "legoanimpresenter.h"
#include "legoworld.h"
#include "misc.h"
#include "misc/legotree.h"
#include "roi/legoroi.h"

#include <vector>

using namespace Extensions::Common;

// Mirrors the game's UpdateStructMapAndROIIndex: assigns ROI indices at runtime
// via SetROIIndex() since m_roiIndex starts at 0 for all animation nodes.
static void AssignROIIndices(
	LegoTreeNode* p_node,
	LegoROI* p_parentROI,
	LegoROI* p_rootROI,
	LegoROI* p_extraROI,
	MxU32& p_nextIndex,
	std::vector<LegoROI*>& p_entries,
	bool& p_rootClaimed
)
{
	LegoROI* roi = p_parentROI;
	LegoAnimNodeData* data = (LegoAnimNodeData*) p_node->GetData();
	const char* name = data ? data->GetName() : nullptr;

	if (name != nullptr && *name != '-') {
		LegoROI* matchedROI = nullptr;

		if (*name == '*' || p_parentROI == nullptr) {
			roi = p_rootROI;
			if (!p_rootClaimed) {
				matchedROI = p_rootROI;
				p_rootClaimed = true;
			}
		}
		else {
			matchedROI = p_parentROI->FindChildROI(name, p_parentROI);
			if (matchedROI == nullptr && p_extraROI != nullptr) {
				matchedROI = p_extraROI->FindChildROI(name, p_extraROI);
			}
		}

		if (matchedROI != nullptr) {
			data->SetROIIndex(p_nextIndex);
			p_entries.push_back(matchedROI);
			p_nextIndex++;
		}
		else {
			data->SetROIIndex(0);
		}
	}

	for (MxS32 i = 0; i < p_node->GetNumChildren(); i++) {
		AssignROIIndices(p_node->GetChild(i), roi, p_rootROI, p_extraROI, p_nextIndex, p_entries, p_rootClaimed);
	}
}

void AnimUtils::BuildROIMap(
	LegoAnim* p_anim,
	LegoROI* p_rootROI,
	LegoROI* p_extraROI,
	LegoROI**& p_roiMap,
	MxU32& p_roiMapSize
)
{
	if (!p_anim || !p_rootROI) {
		return;
	}

	LegoTreeNode* root = p_anim->GetRoot();
	if (!root) {
		return;
	}

	MxU32 nextIndex = 1;
	std::vector<LegoROI*> entries;
	bool rootClaimed = false;
	AssignROIIndices(root, nullptr, p_rootROI, p_extraROI, nextIndex, entries, rootClaimed);

	if (entries.empty()) {
		return;
	}

	// 1-indexed; index 0 reserved as NULL
	p_roiMapSize = entries.size() + 1;
	p_roiMap = new LegoROI*[p_roiMapSize];
	p_roiMap[0] = nullptr;
	for (MxU32 i = 0; i < entries.size(); i++) {
		p_roiMap[i + 1] = entries[i];
	}
}

AnimUtils::AnimCache* AnimUtils::GetOrBuildAnimCache(
	std::map<std::string, AnimCache>& p_cacheMap,
	LegoROI* p_roi,
	const char* p_animName
)
{
	if (!p_animName || !p_roi) {
		return nullptr;
	}

	// Check if already cached
	auto it = p_cacheMap.find(p_animName);
	if (it != p_cacheMap.end()) {
		return &it->second;
	}

	// Look up the animation presenter in the current world
	LegoWorld* world = CurrentWorld();
	if (!world) {
		return nullptr;
	}

	MxCore* presenter = world->Find("LegoAnimPresenter", p_animName);
	if (!presenter) {
		return nullptr;
	}

	LegoAnim* anim = static_cast<LegoAnimPresenter*>(presenter)->GetAnimation();
	if (!anim) {
		return nullptr;
	}

	// Build and cache
	AnimCache& cache = p_cacheMap[p_animName];
	cache.anim = anim;
	BuildROIMap(anim, p_roi, nullptr, cache.roiMap, cache.roiMapSize);

	return &cache;
}
