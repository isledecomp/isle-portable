#pragma once

#include "extensions/extensions.h"
#include "legoworld.h"
#include "mxatom.h"

#include <map>
#include <vector>

namespace Extensions
{
class SiLoader {
public:
	typedef std::pair<MxAtomId, MxU32> StreamObject;

	static void Initialize();
	static bool StartWith(StreamObject p_object);
	static bool RemoveWith(StreamObject p_object, LegoWorld* world);

	static std::map<std::string, std::string> options;
	static bool enabled;

private:
	static std::vector<std::string> files;
	static std::vector<std::pair<StreamObject, StreamObject>> startWith;
	static std::vector<std::pair<StreamObject, StreamObject>> removeWith;

	static bool LoadFile(const char* p_file);
};

#ifdef EXTENSIONS
constexpr auto StartWith = &SiLoader::StartWith;
constexpr auto RemoveWith = &SiLoader::RemoveWith;
#else
constexpr decltype(&SiLoader::StartWith) StartWith = nullptr;
constexpr decltype(&SiLoader::RemoveWith) RemoveWith = nullptr;
#endif
}; // namespace Extensions
