#pragma once

#include "extensions/extensions.h"
#include "legoworld.h"
#include "mxactionnotificationparam.h"
#include "mxatom.h"

#include <map>
#include <vector>

namespace si
{
class Core;
}

namespace Extensions
{
class SiLoader {
public:
	typedef std::pair<MxAtomId, MxU32> StreamObject;

	static void Initialize();
	static bool Load();
	static std::optional<MxCore*> HandleFind(StreamObject p_object, LegoWorld* world);
	static std::optional<MxResult> HandleStart(MxDSAction& p_action);
	static MxBool HandleWorld(LegoWorld* p_world);
	static std::optional<MxBool> HandleRemove(StreamObject p_object, LegoWorld* world);
	static std::optional<MxBool> HandleDelete(MxDSAction& p_action);
	static MxBool HandleEndAction(MxEndActionNotificationParam& p_param);

	template <typename... Args>
	static std::optional<StreamObject> ReplacedIn(MxDSAction& p_action, Args... p_args);

	static std::map<std::string, std::string> options;
	static std::vector<std::string> files;
	static std::vector<std::string> directives;
	static bool enabled;

private:
	static std::vector<std::pair<StreamObject, StreamObject>> startWith;
	static std::vector<std::pair<StreamObject, StreamObject>> removeWith;
	static std::vector<std::pair<StreamObject, StreamObject>> replace;
	static std::vector<std::pair<StreamObject, StreamObject>> prepend;
	static std::vector<StreamObject> fullScreenMovie;
	static std::vector<StreamObject> disable3d;

	static bool LoadFile(const char* p_file);
	static bool LoadDirective(const char* p_directive);
	static MxStreamController* OpenStream(const char* p_file);
	static void ParseExtra(const MxAtomId& p_atom, si::Core* p_core);
	static bool IsWorld(const StreamObject& p_object);
};

#ifdef EXTENSIONS
template <typename... Args>
std::optional<SiLoader::StreamObject> SiLoader::ReplacedIn(MxDSAction& p_action, Args... p_args)
{
	StreamObject object{p_action.GetAtomId(), p_action.GetObjectId()};
	auto checkAtomId = [&p_action, &object](const auto& p_atomId) -> std::optional<StreamObject> {
		for (const auto& key : replace) {
			if (key.second == object && key.first.first == p_atomId) {
				return key.first;
			}
		}

		return std::nullopt;
	};

	std::optional<StreamObject> result;
	((void) (!result.has_value() && (result = checkAtomId(p_args), true)), ...);
	return result;
}

constexpr auto Load = &SiLoader::Load;
constexpr auto HandleFind = &SiLoader::HandleFind;
constexpr auto HandleStart = &SiLoader::HandleStart;
constexpr auto HandleWorld = &SiLoader::HandleWorld;
constexpr auto HandleRemove = &SiLoader::HandleRemove;
constexpr auto HandleDelete = &SiLoader::HandleDelete;
constexpr auto HandleEndAction = &SiLoader::HandleEndAction;
constexpr auto ReplacedIn = [](auto&&... args) { return SiLoader::ReplacedIn(std::forward<decltype(args)>(args)...); };
#else
constexpr decltype(&SiLoader::Load) Load = nullptr;
constexpr decltype(&SiLoader::HandleFind) HandleFind = nullptr;
constexpr decltype(&SiLoader::HandleStart) HandleStart = nullptr;
constexpr decltype(&SiLoader::HandleWorld) HandleWorld = nullptr;
constexpr decltype(&SiLoader::HandleRemove) HandleRemove = nullptr;
constexpr decltype(&SiLoader::HandleDelete) HandleDelete = nullptr;
constexpr decltype(&SiLoader::HandleEndAction) HandleEndAction = nullptr;
constexpr auto ReplacedIn = [](auto&&... args) -> std::optional<SiLoader::StreamObject> {
	((void) args, ...);
	return std::nullopt;
};
#endif
}; // namespace Extensions
