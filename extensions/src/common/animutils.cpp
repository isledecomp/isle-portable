#include "extensions/common/animutils.h"

#include "anim/legoanim.h"
#include "legoanimpresenter.h"
#include "legovideomanager.h"
#include "legoworld.h"
#include "misc.h"
#include "misc/legotree.h"
#include "roi/legoroi.h"
#include "viewmanager/viewlodlist.h"

#include <SDL3/SDL_stdinc.h>
#include <algorithm>
#include <vector>

using namespace Extensions::Common;

// Mirrors the game's UpdateStructMapAndROIIndex: assigns ROI indices at runtime
// via SetROIIndex() since m_roiIndex starts at 0 for all animation nodes.
//
// Intentional divergences from LegoAnimPresenter::BuildROIMap (legoanimpresenter.cpp:413-530):
// 1. No variable substitution -- we bypass the streaming pipeline, so the variable
//    table lacks our entries. Direct name comparison instead.
// 2. *-prefixed nodes search extraROIs -- the original's GetActorName() depends on
//    presenter action context (m_action->GetUnknown24()). We search created extra
//    ROIs directly.
// 3. No LegoAnimStructMap dedup -- sequential indices, functionally correct.
// Look up an animation node name in the alias map (case-insensitive).
static LegoROI* FindAlias(const char* p_name, const AnimUtils::ROIAlias* p_aliases, int p_aliasCount)
{
	for (int i = 0; i < p_aliasCount; i++) {
		if (p_aliases[i].animName && !SDL_strcasecmp(p_name, p_aliases[i].animName)) {
			return p_aliases[i].roi;
		}
	}
	return nullptr;
}

static void AssignROIIndices(
	LegoTreeNode* p_node,
	LegoROI* p_parentROI,
	LegoROI* p_rootROI,
	LegoROI** p_extraROIs,
	int p_extraROICount,
	const AnimUtils::ROIAlias* p_aliases,
	int p_aliasCount,
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

			const char* searchName = (*name == '*') ? name + 1 : name;
			bool matchedExtra = false;

			// Check aliases first (participant ROIs mapped by character name).
			// Claiming root prevents subsequent sibling nodes from also claiming it.
			matchedROI = FindAlias(searchName, p_aliases, p_aliasCount);
			if (matchedROI) {
				roi = matchedROI;
				matchedExtra = true;
				p_rootClaimed = true;
			}

			// Then check extra ROIs by name.
			// This handles cases like BIKESY appearing before SY in the tree:
			// BIKESY should match the vehicle extra, not claim the root.
			if (!matchedExtra && p_extraROICount > 0) {
				for (int e = 0; e < p_extraROICount; e++) {
					matchedROI = p_extraROIs[e]->FindChildROI(searchName, p_extraROIs[e]);
					if (matchedROI != nullptr) {
						roi = matchedROI;
						matchedExtra = true;
						break;
					}
				}
			}

			if (!matchedExtra) {
				if (!p_rootClaimed) {
					matchedROI = p_rootROI;
					p_rootClaimed = true;
				}
			}
		}
		else {
			matchedROI = p_parentROI->FindChildROI(name, p_parentROI);
			if (matchedROI == nullptr) {
				// Check aliases — also update roi so children resolve against the alias ROI
				matchedROI = FindAlias(name, p_aliases, p_aliasCount);
				if (matchedROI) {
					roi = matchedROI;
				}
			}
			if (matchedROI == nullptr) {
				for (int e = 0; e < p_extraROICount; e++) {
					matchedROI = p_extraROIs[e]->FindChildROI(name, p_extraROIs[e]);
					if (matchedROI != nullptr) {
						break;
					}
				}
			}
			// Mirrors original game (legoanimpresenter.cpp:486-490):
			// If FindChildROI fails, the node might be a top-level actor that isn't
			// a child of the current parent. Re-run this node with p_parentROI=NULL
			// so it enters the root-claiming / top-level search path instead.
			if (matchedROI == nullptr) {
				bool isTopLevel = false;
				// Check aliases for top-level match
				if (FindAlias(name, p_aliases, p_aliasCount) != nullptr) {
					isTopLevel = true;
				}
				if (!isTopLevel && !p_rootClaimed && p_rootROI->GetName() &&
					!SDL_strcasecmp(name, p_rootROI->GetName())) {
					isTopLevel = true;
				}
				if (!isTopLevel) {
					for (int e = 0; e < p_extraROICount; e++) {
						if (p_extraROIs[e]->GetName() && !SDL_strcasecmp(name, p_extraROIs[e]->GetName())) {
							isTopLevel = true;
							break;
						}
					}
				}
				if (isTopLevel) {
					AssignROIIndices(
						p_node, nullptr, p_rootROI, p_extraROIs, p_extraROICount,
						p_aliases, p_aliasCount, p_nextIndex, p_entries, p_rootClaimed
					);
					return;
				}
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
		AssignROIIndices(
			p_node->GetChild(i),
			roi,
			p_rootROI,
			p_extraROIs,
			p_extraROICount,
			p_aliases,
			p_aliasCount,
			p_nextIndex,
			p_entries,
			p_rootClaimed
		);
	}
}

void AnimUtils::BuildROIMap(
	LegoAnim* p_anim,
	LegoROI* p_rootROI,
	LegoROI** p_extraROIs,
	int p_extraROICount,
	LegoROI**& p_roiMap,
	MxU32& p_roiMapSize,
	const ROIAlias* p_aliases,
	int p_aliasCount
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
	AssignROIIndices(root, nullptr, p_rootROI, p_extraROIs, p_extraROICount, p_aliases, p_aliasCount, nextIndex, entries, rootClaimed);

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
	BuildROIMap(anim, p_roi, nullptr, 0, cache.roiMap, cache.roiMapSize);

	return &cache;
}

// Read-only tree walk: collect names of animation nodes that don't match the player's ROI hierarchy.
static void CollectUnmatchedNodesRecursive(
	LegoTreeNode* p_node,
	LegoROI* p_parentROI,
	LegoROI* p_rootROI,
	std::vector<std::string>& p_unmatchedNames,
	bool& p_rootClaimed
)
{
	LegoROI* roi = p_parentROI;
	LegoAnimNodeData* data = (LegoAnimNodeData*) p_node->GetData();
	const char* name = data ? data->GetName() : nullptr;

	if (name != nullptr && *name != '-') {
		if (*name == '*' || p_parentROI == nullptr) {
			roi = p_rootROI;
			if (!p_rootClaimed) {
				p_rootClaimed = true;
			}
			else if (*name == '*') {
				// Subsequent *-prefixed node: strip prefix, add lowercased name
				std::string stripped(name + 1);
				std::transform(stripped.begin(), stripped.end(), stripped.begin(), ::tolower);
				if (std::find(p_unmatchedNames.begin(), p_unmatchedNames.end(), stripped) == p_unmatchedNames.end()) {
					p_unmatchedNames.push_back(stripped);
				}
			}
		}
		else {
			LegoROI* matchedROI = p_parentROI->FindChildROI(name, p_parentROI);
			if (matchedROI == nullptr) {
				std::string lowered(name);
				std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);
				if (std::find(p_unmatchedNames.begin(), p_unmatchedNames.end(), lowered) == p_unmatchedNames.end()) {
					p_unmatchedNames.push_back(lowered);
				}
			}
		}
	}

	for (MxS32 i = 0; i < p_node->GetNumChildren(); i++) {
		CollectUnmatchedNodesRecursive(p_node->GetChild(i), roi, p_rootROI, p_unmatchedNames, p_rootClaimed);
	}
}

void AnimUtils::CollectUnmatchedNodes(LegoAnim* p_anim, LegoROI* p_rootROI, std::vector<std::string>& p_unmatchedNames)
{
	if (!p_anim || !p_rootROI) {
		return;
	}

	LegoTreeNode* root = p_anim->GetRoot();
	if (!root) {
		return;
	}

	bool rootClaimed = false;
	CollectUnmatchedNodesRecursive(root, nullptr, p_rootROI, p_unmatchedNames, rootClaimed);
}

void AnimUtils::ApplyTree(LegoAnim* p_anim, MxMatrix& p_transform, LegoTime p_time, LegoROI** p_roiMap)
{
	LegoTreeNode* root = p_anim->GetRoot();
	for (LegoU32 i = 0; i < root->GetNumChildren(); i++) {
		LegoROI::ApplyAnimationTransformation(root->GetChild(i), p_transform, p_time, p_roiMap);
	}
}

LegoROI* AnimUtils::DeepCloneROI(LegoROI* p_source, const char* p_name)
{
	Tgl::Renderer* renderer = VideoManager()->GetRenderer();
	ViewLODList* lodList = reinterpret_cast<ViewLODList*>(const_cast<LODListBase*>(p_source->GetLODs()));

	LegoROI* clone;
	if (lodList && lodList->Size() > 0) {
		clone = new LegoROI(renderer, lodList);
	}
	else {
		clone = new LegoROI(renderer);
	}

	clone->SetName(p_name);
	clone->SetBoundingSphere(p_source->GetBoundingSphere());
	clone->WrappedSetLocal2WorldWithWorldDataUpdate(p_source->GetLocal2World());

	const CompoundObject* children = p_source->GetComp();
	if (children && !children->empty()) {
		CompoundObject* clonedChildren = new CompoundObject();
		for (CompoundObject::const_iterator it = children->begin(); it != children->end(); it++) {
			LegoROI* childSource = (LegoROI*) *it;
			const char* childName = childSource->GetName() ? childSource->GetName() : "";
			LegoROI* childClone = DeepCloneROI(childSource, childName);
			if (childClone) {
				clonedChildren->push_back(childClone);
			}
		}
		clone->SetComp(clonedChildren);
	}

	return clone;
}

// Inverse of an orthonormal affine matrix (rotation + translation).
// R^-1 = R^T, t^-1 = -R^T * t.
static void InvertOrthonormal(MxMatrix& p_out, const MxMatrix& p_in)
{
	p_out.SetIdentity();
	for (int r = 0; r < 3; r++) {
		for (int c = 0; c < 3; c++) {
			p_out[r][c] = p_in[c][r];
		}
	}
	for (int c = 0; c < 3; c++) {
		p_out[3][c] = -(p_in[3][0] * p_out[0][c] + p_in[3][1] * p_out[1][c] + p_in[3][2] * p_out[2][c]);
	}
}

std::vector<MxMatrix> AnimUtils::ComputeChildOffsets(LegoROI* p_parent)
{
	std::vector<MxMatrix> offsets;
	const CompoundObject* children = p_parent->GetComp();
	if (!children) {
		return offsets;
	}

	MxMatrix parentInv;
	InvertOrthonormal(parentInv, p_parent->GetLocal2World());

	for (auto it = children->begin(); it != children->end(); it++) {
		MxMatrix offset;
		offset.Product(parentInv, ((LegoROI*) *it)->GetLocal2World());
		offsets.push_back(offset);
	}

	return offsets;
}

void AnimUtils::ApplyHierarchyTransform(
	LegoROI* p_parent,
	const MxMatrix& p_transform,
	const std::vector<MxMatrix>& p_offsets
)
{
	p_parent->WrappedSetLocal2WorldWithWorldDataUpdate(p_transform);

	const CompoundObject* children = p_parent->GetComp();
	if (!children) {
		return;
	}

	size_t i = 0;
	for (auto it = children->begin(); it != children->end() && i < p_offsets.size(); it++, i++) {
		MxMatrix childWorld;
		childWorld.Product(p_transform, p_offsets[i]);
		((LegoROI*) *it)->WrappedSetLocal2WorldWithWorldDataUpdate(childWorld);
	}
}

std::string AnimUtils::TrimLODSuffix(const std::string& p_name)
{
	std::string result(p_name);
	while (result.size() > 1) {
		char c = result.back();
		if ((c >= '0' && c <= '9') || c == '_') {
			result.pop_back();
		}
		else {
			break;
		}
	}
	return result;
}

const char* AnimUtils::ResolvePropLODName(const char* p_nodeName)
{
	static const struct {
		const char* nodePrefix;
		const char* lodName;
	} mappings[] = {
		{"popmug", "pizpie"},
	};

	for (const auto& m : mappings) {
		if (!SDL_strncasecmp(p_nodeName, m.nodePrefix, SDL_strlen(m.nodePrefix))) {
			return m.lodName;
		}
	}
	return p_nodeName;
}
