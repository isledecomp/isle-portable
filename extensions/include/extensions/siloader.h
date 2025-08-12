#pragma once

#include "extensions/extensions.h"
#include "legoworld.h"
#include "mxatom.h"

#include <interleaf.h>
#include <map>
#include <vector>

namespace Extensions
{
class SiLoader {
public:
	typedef std::pair<MxAtomId, MxU32> StreamObject;

	static void Initialize();
	static bool Load();
	static std::optional<MxCore*> HandleFind(StreamObject p_object, LegoWorld* world);
	static std::optional<MxResult> HandleStart(StreamObject p_object);
	static std::optional<MxBool> HandleRemove(StreamObject p_object, LegoWorld* world);
	static std::optional<MxBool> HandleDelete(StreamObject p_object);

	static std::map<std::string, std::string> options;
	static std::vector<std::string> files;
	static bool enabled;

private:
	static std::vector<std::pair<StreamObject, StreamObject>> startWith;
	static std::vector<std::pair<StreamObject, StreamObject>> removeWith;
	static std::vector<std::pair<StreamObject, StreamObject>> replace;

	static bool LoadFile(const char* p_file);
	static void ParseDirectives(const MxAtomId& p_atom, si::Core* p_core, MxAtomId p_parentReplacedAtom = MxAtomId());
};

#ifdef EXTENSIONS
constexpr auto Load = &SiLoader::Load;
constexpr auto HandleFind = &SiLoader::HandleFind;
constexpr auto HandleStart = &SiLoader::HandleStart;
constexpr auto HandleRemove = &SiLoader::HandleRemove;
constexpr auto HandleDelete = &SiLoader::HandleDelete;
#else
constexpr decltype(&SiLoader::Load) Load = nullptr;
constexpr decltype(&SiLoader::HandleFind) HandleFind = nullptr;
constexpr decltype(&SiLoader::HandleStart) HandleStart = nullptr;
constexpr decltype(&SiLoader::HandleRemove) HandleRemove = nullptr;
constexpr decltype(&SiLoader::HandleDelete) HandleDelete = nullptr;
#endif
}; // namespace Extensions
