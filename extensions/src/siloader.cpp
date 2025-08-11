#include "extensions/siloader.h"

#include "mxdsaction.h"
#include "mxmisc.h"
#include "mxstreamer.h"

#include <SDL3/SDL.h>

using namespace Extensions;

std::map<std::string, std::string> SiLoader::options;
std::vector<std::string> SiLoader::files;
std::vector<std::pair<SiLoader::StreamObject, SiLoader::StreamObject>> SiLoader::startWith;
std::vector<std::pair<SiLoader::StreamObject, SiLoader::StreamObject>> SiLoader::removeWith;
std::vector<std::pair<SiLoader::StreamObject, SiLoader::StreamObject>> SiLoader::replace;
bool SiLoader::enabled = false;

void SiLoader::Initialize()
{
	char* files = SDL_strdup(options["si loader:files"].c_str());
	char* saveptr;

	for (char* file = SDL_strtok_r(files, ",\n", &saveptr); file; file = SDL_strtok_r(NULL, ",\n", &saveptr)) {
		SiLoader::files.emplace_back(file);
	}

	SDL_free(files);
}

bool SiLoader::Load()
{
	for (const auto& file : files) {
		LoadFile(file.c_str());
	}

	return true;
}

std::optional<MxResult> SiLoader::HandleStart(StreamObject p_object)
{
	for (const auto& key : startWith) {
		if (key.first == p_object) {
			MxDSAction action;
			action.SetAtomId(key.second.first);
			action.SetObjectId(key.second.second);
			Start(&action);
		}
	}

	for (const auto& key : replace) {
		if (key.first == p_object) {
			MxDSAction action;
			action.SetAtomId(key.second.first);
			action.SetObjectId(key.second.second);
			return Start(&action);
		}
	}

	return std::nullopt;
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

bool SiLoader::LoadFile(const char* p_file)
{
	si::Interleaf si;
	MxStreamController* controller;

	MxString path = MxString(MxOmni::GetHD()) + p_file;
	path.MapPathToFilesystem();
	if (si.Read(path.GetData()) != si::Interleaf::ERROR_SUCCESS) {
		path = MxString(MxOmni::GetCD()) + p_file;
		path.MapPathToFilesystem();
		if (si.Read(path.GetData()) != si::Interleaf::ERROR_SUCCESS) {
			SDL_Log("Could not parse SI file %s", p_file);
			return false;
		}
	}

	if (!(controller = Streamer()->Open(p_file, MxStreamer::e_diskStream))) {
		SDL_Log("Could not load SI file %s", p_file);
		return false;
	}

	ParseDirectives(controller->GetAtom(), &si);
	return true;
}

void SiLoader::ParseDirectives(const MxAtomId& p_atom, si::Core* p_core, const MxAtomId* p_parentReplacedAtom)
{
	for (si::Core* child : p_core->GetChildren()) {
		if (si::Object* object = dynamic_cast<si::Object*>(child)) {
			if (object->type() != si::MxOb::Null) {
				std::string extra(object->extra_.data(), object->extra_.size());
				const char* directive;
				char atom[256];
				uint32_t id;

				if ((directive = SDL_strstr(extra.c_str(), "StartWith:"))) {
					if (SDL_sscanf(directive, "StartWith:%255[^;];%d", atom, &id) == 2) {
						startWith.emplace_back(
							StreamObject{MxAtomId{atom, e_lowerCase2}, id},
							StreamObject{p_atom, object->id_}
						);
					}
				}

				if ((directive = SDL_strstr(extra.c_str(), "RemoveWith:"))) {
					if (SDL_sscanf(directive, "RemoveWith:%255[^;];%d", atom, &id) == 2) {
						removeWith.emplace_back(
							StreamObject{MxAtomId{atom, e_lowerCase2}, id},
							StreamObject{p_atom, object->id_}
						);
					}
				}

				if (p_parentReplacedAtom) {
					replace.emplace_back(StreamObject{*p_parentReplacedAtom, id}, StreamObject{p_atom, object->id_});
				}
				else {
					if ((directive = SDL_strstr(extra.c_str(), "Replace:"))) {
						if (SDL_sscanf(directive, "Replace:%255[^;];%d", atom, &id) == 2) {
							replace.emplace_back(
								StreamObject{MxAtomId{atom, e_lowerCase2}, id},
								StreamObject{p_atom, object->id_}
							);
							p_parentReplacedAtom = &replace.back().first.first;
						}
					}
				}
			}
		}

		ParseDirectives(p_atom, child, p_parentReplacedAtom);
	}
}
