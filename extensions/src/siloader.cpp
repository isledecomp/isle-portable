#include "extensions/siloader.h"

#include "mxdsaction.h"
#include "mxmisc.h"
#include "mxstreamer.h"

#include <SDL3/SDL.h>
#include <interleaf.h>
#include <algorithm>

using namespace Extensions;

const char prependedMarker[] = ";;prepended;;";

std::map<std::string, std::string> SiLoader::options;
std::vector<std::string> SiLoader::files;
std::vector<std::string> SiLoader::directives;
std::vector<std::pair<SiLoader::StreamObject, SiLoader::StreamObject>> SiLoader::startWith;
std::vector<std::pair<SiLoader::StreamObject, SiLoader::StreamObject>> SiLoader::removeWith;
std::vector<std::pair<SiLoader::StreamObject, SiLoader::StreamObject>> SiLoader::replace;
std::vector<std::pair<SiLoader::StreamObject, SiLoader::StreamObject>> SiLoader::prepend;
std::vector<SiLoader::StreamObject> SiLoader::fullScreenMovie;
std::vector<SiLoader::StreamObject> SiLoader::disable3d;
bool SiLoader::enabled = false;

void SiLoader::Initialize()
{
	char* files = SDL_strdup(options["si loader:files"].c_str());
	char* saveptr;

	for (char* file = SDL_strtok_r(files, ",\n\r ", &saveptr); file; file = SDL_strtok_r(NULL, ",\n\r ", &saveptr)) {
		SiLoader::files.emplace_back(file);
	}

	char* directives = SDL_strdup(options["si loader:directives"].c_str());

	for (char* directive = SDL_strtok_r(directives, ",\n\r ", &saveptr); directive;
		 directive = SDL_strtok_r(NULL, ",\n\r ", &saveptr)) {
		SiLoader::directives.emplace_back(directive);
	}

	SDL_free(files);
	SDL_free(directives);
}

bool SiLoader::Load()
{
	for (const auto& file : files) {
		LoadFile(file.c_str());
	}

	for (const auto& directive : directives) {
		LoadDirective(directive.c_str());
	}

	return true;
}

std::optional<MxCore*> SiLoader::HandleFind(StreamObject p_object, LegoWorld* world)
{
	for (const auto& key : replace) {
		if (key.first == p_object) {
			return world->Find(key.second.first, key.second.second);
		}
	}

	return std::nullopt;
}

std::optional<MxResult> SiLoader::HandleStart(MxDSAction& p_action)
{
	StreamObject object{p_action.GetAtomId(), p_action.GetObjectId()};
	auto start = [](const StreamObject& p_object, MxDSAction& p_in, MxDSAction& p_out) -> MxResult {
		if (!OpenStream(p_object.first.GetInternal())) {
			return FAILURE;
		}

		p_out.SetAtomId(p_object.first);
		p_out.SetObjectId(p_object.second);
		p_out.SetUnknown24(p_in.GetUnknown24());
		p_out.SetNotificationObject(p_in.GetNotificationObject());
		p_out.SetOrigin(p_in.GetOrigin());
		return Start(&p_out);
	};

	for (const auto& key : startWith) {
		if (key.first == object && !IsWorld(key.first)) {
			MxDSAction action;
			start(key.second, p_action, action);
		}
	}

	for (const auto& key : replace) {
		if (key.first == object) {
			MxDSAction action;
			MxResult result = start(key.second, p_action, action);

			if (result == SUCCESS) {
				p_action.SetUnknown24(action.GetUnknown24());
			}

			return result;
		}
	}

	if (p_action.GetExtraLength() == 0 || !SDL_strstr(p_action.GetExtraData(), prependedMarker)) {
		for (const auto& key : prepend) {
			if (key.first == object) {
				MxDSAction action;
				MxResult result = start(key.second, p_action, action);

				if (result == SUCCESS) {
					p_action.SetUnknown24(action.GetUnknown24());
				}

				return result;
			}
		}
	}

	if (std::find(fullScreenMovie.begin(), fullScreenMovie.end(), object) != fullScreenMovie.end()) {
		VideoManager()->EnableFullScreenMovie(TRUE);
	}

	if (std::find(disable3d.begin(), disable3d.end(), object) != disable3d.end()) {
		VideoManager()->FUN_1007c520();
	}

	return std::nullopt;
}

MxBool SiLoader::HandleWorld(LegoWorld* p_world)
{
	StreamObject object{p_world->GetAtomId(), p_world->GetEntityId()};
	auto start = [](const StreamObject& p_object, MxDSAction& p_out) {
		if (!OpenStream(p_object.first.GetInternal())) {
			return;
		}

		p_out.SetAtomId(p_object.first);
		p_out.SetObjectId(p_object.second);
		p_out.SetUnknown24(-1);
		Start(&p_out);
	};

	for (const auto& key : startWith) {
		if (key.first == object) {
			MxDSAction action;
			start(key.second, action);
		}
	}

	return TRUE;
}

std::optional<MxBool> SiLoader::HandleRemove(StreamObject p_object, LegoWorld* world)
{
	for (const auto& key : removeWith) {
		if (key.first == p_object) {
			RemoveFromWorld(key.second.first, key.second.second, world->GetAtomId(), world->GetEntityId());
		}
	}

	for (const auto& key : replace) {
		if (key.first == p_object) {
			return RemoveFromWorld(key.second.first, key.second.second, world->GetAtomId(), world->GetEntityId());
		}
	}

	return std::nullopt;
}

std::optional<MxBool> SiLoader::HandleDelete(MxDSAction& p_action)
{
	StreamObject object{p_action.GetAtomId(), p_action.GetObjectId()};
	auto deleteObject = [](const StreamObject& p_object, MxDSAction& p_in, MxDSAction& p_out) {
		p_out.SetAtomId(p_object.first);
		p_out.SetObjectId(p_object.second);
		p_out.SetUnknown24(p_in.GetUnknown24());
		p_out.SetNotificationObject(p_in.GetNotificationObject());
		p_out.SetOrigin(p_in.GetOrigin());
		DeleteObject(p_out);
	};

	for (const auto& key : removeWith) {
		if (key.first == object) {
			MxDSAction action;
			deleteObject(key.second, p_action, action);
		}
	}

	for (const auto& key : replace) {
		if (key.first == object) {
			MxDSAction action;
			deleteObject(key.second, p_action, action);
			p_action.SetUnknown24(action.GetUnknown24());
			return TRUE;
		}
	}

	return std::nullopt;
}

MxBool SiLoader::HandleEndAction(MxEndActionNotificationParam& p_param)
{
	StreamObject object{p_param.GetAction()->GetAtomId(), p_param.GetAction()->GetObjectId()};
	auto start = [](const StreamObject& p_object, MxDSAction& p_in, MxDSAction& p_out) -> MxResult {
		if (!OpenStream(p_object.first.GetInternal())) {
			return FAILURE;
		}

		p_out.SetAtomId(p_object.first);
		p_out.SetObjectId(p_object.second);
		p_out.SetUnknown24(-1);
		p_out.SetNotificationObject(p_in.GetNotificationObject());
		p_out.SetOrigin(p_in.GetOrigin());
		p_out.AppendExtra(sizeof(prependedMarker), prependedMarker);
		return Start(&p_out);
	};

	if (std::find(fullScreenMovie.begin(), fullScreenMovie.end(), object) != fullScreenMovie.end()) {
		VideoManager()->EnableFullScreenMovie(FALSE);
	}

	if (!p_param.GetSender() || !p_param.GetSender()->IsA("MxCompositePresenter")) {
		for (const auto& key : prepend) {
			if (key.second == object) {
				MxDSAction action;
				start(key.first, *p_param.GetAction(), action);
			}
		}
	}

	return TRUE;
}

bool SiLoader::LoadFile(const char* p_file)
{
	si::Interleaf si;
	MxStreamController* controller;

	MxString path = MxString(MxOmni::GetHD()) + p_file;
	path.MapPathToFilesystem();
	if (si.Read(path.GetData(), si::Interleaf::ObjectsOnly | si::Interleaf::NoInfo) != si::Interleaf::ERROR_SUCCESS) {
		path = MxString(MxOmni::GetCD()) + p_file;
		path.MapPathToFilesystem();
		if (si.Read(path.GetData(), si::Interleaf::ObjectsOnly | si::Interleaf::NoInfo) !=
			si::Interleaf::ERROR_SUCCESS) {
			SDL_Log("Could not parse SI file %s", p_file);
			return false;
		}
	}

	if (!(controller = OpenStream(p_file))) {
		return false;
	}

	ParseExtra(controller->GetAtom(), &si);
	return true;
}

bool SiLoader::LoadDirective(const char* p_directive)
{
	char originAtom[256], targetAtom[256];
	uint32_t originId, targetId;

	if (SDL_sscanf(
			p_directive,
			"StartWith:%255[^:;]%*[:;]%u%*[:;]%255[^:;]%*[:;]%u",
			originAtom,
			&originId,
			targetAtom,
			&targetId
		) == 4) {
		startWith.emplace_back(
			StreamObject{MxAtomId{originAtom, e_lowerCase2}, originId},
			StreamObject{MxAtomId{targetAtom, e_lowerCase2}, targetId}
		);
	}
	else if (SDL_sscanf(p_directive, "RemoveWith:%255[^:;]%*[:;]%u%*[:;]%255[^:;]%*[:;]%u", originAtom, &originId, targetAtom, &targetId) == 4) {
		removeWith.emplace_back(
			StreamObject{MxAtomId{originAtom, e_lowerCase2}, originId},
			StreamObject{MxAtomId{targetAtom, e_lowerCase2}, targetId}
		);
	}
	else if (SDL_sscanf(p_directive, "Replace:%255[^:;]%*[:;]%u%*[:;]%255[^:;]%*[:;]%u", originAtom, &originId, targetAtom, &targetId) == 4) {
		replace.emplace_back(
			StreamObject{MxAtomId{originAtom, e_lowerCase2}, originId},
			StreamObject{MxAtomId{targetAtom, e_lowerCase2}, targetId}
		);
	}
	else if (SDL_sscanf(p_directive, "Prepend:%255[^:;]%*[:;]%u%*[:;]%255[^:;]%*[:;]%u", originAtom, &originId, targetAtom, &targetId) == 4) {
		prepend.emplace_back(
			StreamObject{MxAtomId{targetAtom, e_lowerCase2}, targetId},
			StreamObject{MxAtomId{originAtom, e_lowerCase2}, originId}
		);
	}
	else if (SDL_sscanf(p_directive, "FullScreenMovie:%255[^:;]%*[:;]%u", originAtom, &originId) == 2) {
		fullScreenMovie.emplace_back(StreamObject{MxAtomId{originAtom, e_lowerCase2}, originId});
	}
	else if (SDL_sscanf(p_directive, "Disable3d:%255[^:;]%*[:;]%u", originAtom, &originId) == 2) {
		disable3d.emplace_back(StreamObject{MxAtomId{originAtom, e_lowerCase2}, originId});
	}

	return true;
}

MxStreamController* SiLoader::OpenStream(const char* p_file)
{
	MxStreamController* controller;

	if (!(controller = Streamer()->Open(p_file, MxStreamer::e_diskStream))) {
		SDL_Log("Could not load SI file %s", p_file);
		return nullptr;
	}

	return controller;
}

void SiLoader::ParseExtra(const MxAtomId& p_atom, si::Core* p_core)
{
	for (si::Core* child : p_core->GetChildren()) {
		if (si::Object* object = dynamic_cast<si::Object*>(child)) {
			if (object->type() != si::MxOb::Null) {
				std::string extra(object->extra_.data(), object->extra_.size());
				const char* directive;
				char atom[256];
				uint32_t id;

				if ((directive = SDL_strstr(extra.c_str(), "StartWith:"))) {
					if (SDL_sscanf(directive, "StartWith:%255[^:;]%*[:;]%u", atom, &id) == 2) {
						startWith.emplace_back(
							StreamObject{MxAtomId{atom, e_lowerCase2}, id},
							StreamObject{p_atom, object->id_}
						);
					}
				}

				if ((directive = SDL_strstr(extra.c_str(), "RemoveWith:"))) {
					if (SDL_sscanf(directive, "RemoveWith:%255[^:;]%*[:;]%u", atom, &id) == 2) {
						removeWith.emplace_back(
							StreamObject{MxAtomId{atom, e_lowerCase2}, id},
							StreamObject{p_atom, object->id_}
						);
					}
				}

				if ((directive = SDL_strstr(extra.c_str(), "Replace:"))) {
					if (SDL_sscanf(directive, "Replace:%255[^:;]%*[:;]%u", atom, &id) == 2) {
						replace.emplace_back(
							StreamObject{MxAtomId{atom, e_lowerCase2}, id},
							StreamObject{p_atom, object->id_}
						);
					}
				}

				if ((directive = SDL_strstr(extra.c_str(), "Prepend:"))) {
					if (SDL_sscanf(directive, "Prepend:%255[^:;]%*[:;]%u", atom, &id) == 2) {
						prepend.emplace_back(
							StreamObject{p_atom, object->id_},
							StreamObject{MxAtomId{atom, e_lowerCase2}, id}
						);
					}
				}

				if ((directive = SDL_strstr(extra.c_str(), "FullScreenMovie"))) {
					fullScreenMovie.emplace_back(StreamObject{MxAtomId{atom, e_lowerCase2}, id});
				}

				if ((directive = SDL_strstr(extra.c_str(), "Disable3d"))) {
					disable3d.emplace_back(StreamObject{MxAtomId{atom, e_lowerCase2}, id});
				}
			}
		}

		ParseExtra(p_atom, child);
	}
}

bool SiLoader::IsWorld(const StreamObject& p_object)
{
	// The convention in LEGO Island is that world objects are always at ID 0
	if (p_object.second == 0) {
		for (int i = 0; i < LegoOmni::e_numWorlds; i++) {
			if (p_object.first == *Lego()->GetWorldAtom((LegoOmni::World) i)) {
				return true;
			}
		}
	}

	return false;
}
