#include "extensions/multiplayer/emoteanimhandler.h"

#include "3dmanager/lego3dmanager.h"
#include "anim/legoanim.h"
#include "extensions/common/animutils.h"
#include "legocharactermanager.h"
#include "legovideomanager.h"
#include "misc.h"
#include "misc/legotree.h"
#include "roi/legoroi.h"

#include <SDL3/SDL_stdinc.h>
#include <algorithm>
#include <string>
#include <vector>

using namespace Multiplayer;
namespace AnimUtils = Extensions::Common::AnimUtils;

// Emote table. Each entry has two phases: {anim, sound}.
// Phase 2 anim is nullptr for one-shot emotes; non-null makes it a multi-part stateful emote.
const EmoteEntry Multiplayer::g_emoteEntries[] = {
	{{{"CNs011xx", nullptr}, {nullptr, nullptr}}},     // 0: Wave (one-shot)
	{{{"CNs012xx", nullptr}, {nullptr, nullptr}}},     // 1: Hat Tip (one-shot)
	{{{"BNsDis01", "crash5"}, {"BNsAss01", nullptr}}}, // 2: Disassemble / Reassemble (multi-part)
	{{{"CNs008Br", nullptr}, {nullptr, nullptr}}},     // 3: Look Around (one-shot)
	{{{"CNs014Br", nullptr}, {nullptr, nullptr}}},     // 4: Headless (one-shot)
	{{{"CNs013Pa", nullptr}, {nullptr, nullptr}}},     // 5: Toss (one-shot)
};
const int Multiplayer::g_emoteAnimCount = sizeof(g_emoteEntries) / sizeof(g_emoteEntries[0]);

bool EmoteAnimHandler::IsValid(uint8_t p_id) const
{
	return p_id < g_emoteAnimCount;
}

bool EmoteAnimHandler::IsMultiPart(uint8_t p_id) const
{
	return IsMultiPartEmote(p_id);
}

const char* EmoteAnimHandler::GetAnimName(uint8_t p_id, int p_phase) const
{
	if (p_id >= g_emoteAnimCount || p_phase < 0 || p_phase > 1) {
		return nullptr;
	}
	return g_emoteEntries[p_id].phases[p_phase].anim;
}

const char* EmoteAnimHandler::GetSoundName(uint8_t p_id, int p_phase) const
{
	if (p_id >= g_emoteAnimCount || p_phase < 0 || p_phase > 1) {
		return nullptr;
	}
	return g_emoteEntries[p_id].phases[p_phase].sound;
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

static void CollectUnmatchedNodes(LegoAnim* p_anim, LegoROI* p_rootROI, std::vector<std::string>& p_unmatchedNames)
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

// Resolve a prop node name to its LOD name.
static const char* ResolvePropLODName(const char* p_nodeName)
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

void EmoteAnimHandler::BuildProps(
	Extensions::Common::PropGroup& p_group,
	LegoAnim* p_anim,
	LegoROI* p_playerROI,
	uint32_t p_propSuffix
)
{
	std::vector<std::string> unmatchedNames;
	CollectUnmatchedNodes(p_anim, p_playerROI, unmatchedNames);
	if (unmatchedNames.empty()) {
		return;
	}

	std::vector<LegoROI*> createdROIs;
	for (const std::string& name : unmatchedNames) {
		char uniqueName[64];
		if (p_propSuffix != 0) {
			SDL_snprintf(uniqueName, sizeof(uniqueName), "%s_mp_%u", name.c_str(), p_propSuffix);
		}
		else {
			SDL_snprintf(uniqueName, sizeof(uniqueName), "tp_prop_%s", name.c_str());
		}

		const char* lodName = ResolvePropLODName(name.c_str());
		LegoROI* propROI = CharacterManager()->CreateAutoROI(uniqueName, lodName, FALSE);
		if (propROI) {
			propROI->SetName(name.c_str());
			createdROIs.push_back(propROI);
		}
	}

	if (createdROIs.empty()) {
		return;
	}

	p_group.propCount = (uint8_t) createdROIs.size();
	p_group.propROIs = new LegoROI*[p_group.propCount];
	for (uint8_t i = 0; i < p_group.propCount; i++) {
		p_group.propROIs[i] = createdROIs[i];
	}

	AnimUtils::BuildROIMap(
		p_anim,
		p_playerROI,
		p_group.propROIs,
		p_group.propCount,
		p_group.roiMap,
		p_group.roiMapSize
	);
}
