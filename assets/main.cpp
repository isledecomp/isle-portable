#include "mxdsaction.h"

#include <cstring>
#include <file.h>
#include <filesystem>
#include <fstream>
#include <interleaf.h>
#include <map>
#include <object.h>
#include <othertypes.h>
#include <vector>

void ZeroInitObject(si::Object* o)
{
	o->unknown1_ = 0;
	o->flags_ = 0;
	o->unknown4_ = 0;
	o->duration_ = 0;
	o->loops_ = 0;
	o->unknown26_ = 0;
	o->unknown27_ = 0;
	o->unknown28_ = 0;
	o->unknown29_ = 0;
	o->unknown30_ = 0;
	o->volume_ = 0;
	o->time_offset_ = 0;
}

si::Interleaf::Version version = si::Interleaf::Version2_2;
uint32_t bufferSize = 65536;
uint32_t bufferCount = 8;

std::string out;
std::ofstream depfile;
si::MemoryBuffer mxHd;

void CreateWidescreen()
{
	std::string result = out + "/widescreen.si";
	struct AssetView {
		std::string name;
		std::string extra;
		int32_t z;
	};
	const AssetView widescreenBitmaps[] = {
		{"GaraDoor_Background_Wide",
		 "World:current, StartWith:\\Lego\\Scripts\\Isle\\Isle;1160, RemoveWith:\\Lego\\Scripts\\Isle\\Isle;1161",
		 -10},
		{"Police_Background_Wide",
		 "World:\\lego\\scripts\\police\\police;0, StartWith:\\Lego\\Scripts\\Police\\Police;0, "
		 "RemoveWith:\\Lego\\Scripts\\Police\\Police;0",
		 10},
		{"Hospital_Background_Wide",
		 "World:\\lego\\scripts\\hospital\\hospital;0, StartWith:\\Lego\\Scripts\\Hospital\\Hospital;0, "
		 "RemoveWith:\\Lego\\Scripts\\Hospital\\Hospital;0",
		 10},
		{"SeaView_Background_Wide",
		 "World:\\Lego\\Scripts\\Isle\\Isle;0, StartWith:\\Lego\\Scripts\\Isle\\Isle;1140, "
		 "RemoveWith:\\Lego\\Scripts\\Isle\\Isle;1141",
		 -10},
		{"ElevRide_Background_Wide",
		 "World:\\Lego\\Scripts\\Isle\\Isle;0, StartWith:\\Lego\\Scripts\\Isle\\Isle;1050, "
		 "RemoveWith:\\Lego\\Scripts\\Isle\\Isle;1051",
		 -10},
		{"Observe_Background_Wide",
		 "World:current, StartWith:\\Lego\\Scripts\\Isle\\Isle;1118, "
		 "RemoveWith:\\Lego\\Scripts\\Isle\\Isle;1119",
		 -10},
		{"ElevDown_Background_Wide",
		 "World:\\Lego\\Scripts\\Isle\\Isle;0, StartWith:\\Lego\\Scripts\\Isle\\Isle;1145, "
		 "RemoveWith:\\Lego\\Scripts\\Isle\\Isle;1146",
		 -10},
		{"ElevOpen_Background_Wide",
		 "World:\\Lego\\Scripts\\Isle\\Isle;0, StartWith:\\Lego\\Scripts\\Isle\\Isle;1114, "
		 "RemoveWith:\\Lego\\Scripts\\Isle\\Isle;1115",
		 -10},
		{"PoliDoor_Background_Wide",
		 "World:\\Lego\\Scripts\\Isle\\Isle;0, StartWith:\\Lego\\Scripts\\Isle\\Isle;1150, "
		 "RemoveWith:\\Lego\\Scripts\\Isle\\Isle;1151",
		 -10},
		{"InfoScor_Background_Wide",
		 "World:\\Lego\\Scripts\\Infocntr\\INFOSCOR;0, StartWith:\\Lego\\Scripts\\Infocntr\\INFOSCOR;0, "
		 "RemoveWith:\\Lego\\Scripts\\Infocntr\\INFOSCOR;0",
		 10},
		{"RegBook_Background_Wide",
		 "World:\\Lego\\Scripts\\Infocntr\\REGBOOK;0, StartWith:\\Lego\\Scripts\\Infocntr\\REGBOOK;0, "
		 "RemoveWith:\\Lego\\Scripts\\Infocntr\\REGBOOK;0",
		 10},
		{"HistBook_Background_Wide",
		 "StartWith:\\Lego\\Scripts\\Infocntr\\HISTBOOK;0, RemoveWith:\\Lego\\Scripts\\Infocntr\\HISTBOOK;0",
		 10},
		{"InfoMain_Background_Wide",
		 "World:\\Lego\\Scripts\\Infocntr\\INFOMAIN;0, StartWith:\\Lego\\Scripts\\Infocntr\\INFOMAIN;0, "
		 "RemoveWith:\\Lego\\Scripts\\Infocntr\\INFOMAIN;0",
		 10},
		{"InfoMain_BackgroundRed_Wide", "Visibility:FALSE", 10},
		{"ElevBott_Background_Wide",
		 "World:\\Lego\\Scripts\\Infocntr\\ELEVBOTT;0, StartWith:\\Lego\\Scripts\\Infocntr\\ELEVBOTT;0, "
		 "RemoveWith:\\Lego\\Scripts\\Infocntr\\ELEVBOTT;0",
		 10},
		{"InfoDoor_Background_Wide",
		 "World:\\Lego\\Scripts\\Infocntr\\INFODOOR;0, StartWith:\\Lego\\Scripts\\Infocntr\\INFODOOR;0, "
		 "RemoveWith:\\Lego\\Scripts\\Infocntr\\INFODOOR;0",
		 10},
		{"RaceCar_Background_Wide",
		 "World:\\Lego\\Scripts\\Build\\RACECAR;0, StartWith:\\Lego\\Scripts\\Build\\RACECAR;0, "
		 "RemoveWith:\\Lego\\Scripts\\Build\\RACECAR;0",
		 10},
		{"DuneCar_Background_Wide",
		 "World:\\Lego\\Scripts\\Build\\DUNECAR;0, StartWith:\\Lego\\Scripts\\Build\\DUNECAR;0, "
		 "RemoveWith:\\Lego\\Scripts\\Build\\DUNECAR;0",
		 10},
		{"Copter_Background_Wide",
		 "World:\\Lego\\Scripts\\Build\\COPTER;0, StartWith:\\Lego\\Scripts\\Build\\COPTER;0, "
		 "RemoveWith:\\Lego\\Scripts\\Build\\COPTER;0",
		 10},
		{"JetSki_Background_Wide",
		 "World:\\Lego\\Scripts\\Build\\JETSKI;0, StartWith:\\Lego\\Scripts\\Build\\JETSKI;0, "
		 "RemoveWith:\\Lego\\Scripts\\Build\\JETSKI;0",
		 10},
		{"Garage_Background_Wide",
		 "World:\\Lego\\Scripts\\Garage\\GARAGE;0, StartWith:\\Lego\\Scripts\\Garage\\GARAGE;0, "
		 "RemoveWith:\\Lego\\Scripts\\Garage\\GARAGE;0",
		 10},
		{"JukeBox_Background_Wide",
		 "StartWith:\\Lego\\Scripts\\Isle\\JUKEBOXW;0, RemoveWith:\\Lego\\Scripts\\Isle\\JUKEBOXW;0",
		 10}
	};

	si::Interleaf si;
	mxHd.seek(0, si::MemoryBuffer::SeekStart);
	si.Read(&mxHd);

	int i = 0;
	for (const AssetView& asset : widescreenBitmaps) {
		si::Object* object = new si::Object;
		std::string file = std::string("widescreen/") + asset.name + ".bmp";

		object->id_ = i;
		object->type_ = si::MxOb::Bitmap;
		object->flags_ = MxDSAction::c_enabled | MxDSAction::c_bit4;
		object->duration_ = -1;
		object->loops_ = 1;
		object->extra_ = si::bytearray(asset.extra.c_str(), asset.extra.length() + 1);
		object->presenter_ = "MxStillPresenter";
		object->name_ = asset.name;
		object->filetype_ = si::MxOb::STL;
		object->location_ = si::Vector3(-240, -528, asset.z);
		object->direction_ = si::Vector3(0, 0, 0);
		object->up_ = si::Vector3(0, 1, 0);

		if (!object->ReplaceWithFile(file.c_str())) {
			abort();
		}

		si.AppendChild(object);
		depfile << result << ": " << (std::filesystem::current_path() / file).string() << std::endl;
		i++;
	}

	si.Write(result.c_str());
}

void CreateHDMusic()
{
	std::string result = out + "/hdmusic.si";
	struct AssetView {
		std::string name;
		std::string extra;
		uint32_t duration;
		uint32_t loops;
		uint32_t flags;
	};
	const AssetView wavAudio[] = {
		{"BrickstrChase_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;3",
		 82850,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3},
		{"BrickHunt_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;4",
		 192630,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3},
		{"ResidentalArea_Music_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;5",
		 89540,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3},
		{"BeachBlvd_Music_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;6",
		 152520,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3},
		{"Cave_Music_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;7",
		 69240,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3},
		{"CentralRoads_Music_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;8",
		 188300,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3},
		{"Jail_Music_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;9",
		 66948,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3},
		{"Hospital_Music_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;10",
		 211990,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3},
		{"InformationCenter_Music_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;11",
		 150625,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3},
		{"PoliceStation_Music_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;12",
		 57090,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3},
		{"Park_Music_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;13",
		 92169,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3},
		{"RaceTrackRoad_Music_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;16",
		 189000,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3},
		{"Beach_Music_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;17",
		 127490,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3},
		{"JetskiRace_Music_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;19",
		 64440,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3},
		{"Act3Music_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;20",
		 80510,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3},
		{"JBMusic1_HD", "Replace:\\Lego\\Scripts\\Isle\\Jukebox;55", 125850, 1, MxDSAction::c_enabled},
		{"JBMusic2_HD", "Replace:\\Lego\\Scripts\\Isle\\Jukebox;56", 162900, 1, MxDSAction::c_enabled},
		{"JBMusic3_HD", "Replace:\\Lego\\Scripts\\Isle\\Jukebox;57", 122750, 1, MxDSAction::c_enabled},
		{"JBMusic4_HD", "Replace:\\Lego\\Scripts\\Isle\\Jukebox;58", 140000, 1, MxDSAction::c_enabled},
		{"JBMusic5_HD", "Replace:\\Lego\\Scripts\\Isle\\Jukebox;59", 72720, 1, MxDSAction::c_enabled},
		{"JBMusic6_HD", "Replace:\\Lego\\Scripts\\Isle\\Jukebox;60", 57030, 1, MxDSAction::c_enabled},
		{"InfoCenter_3rd_Floor_Music_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;61",
		 114520,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3},
		{"MusicTheme1_HD", "Replace:\\Lego\\Scripts\\Isle\\Jukebox;0", 128282, 1, MxDSAction::c_enabled},
		{"PizzaMission_Music_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;63",
		 122078,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3}
	};

	si::Interleaf si;
	mxHd.seek(0, si::MemoryBuffer::SeekStart);
	si.Read(&mxHd);

	int i = 0;
	for (const AssetView& asset : wavAudio) {
		si::Object* object = new si::Object;
		std::string file = std::string("hdmusic/") + asset.name + ".wav";

		object->id_ = i;
		object->type_ = si::MxOb::Sound;
		object->flags_ = asset.flags;
		object->duration_ = asset.duration * asset.loops;
		object->loops_ = asset.loops;
		object->extra_ = si::bytearray(asset.extra.c_str(), asset.extra.length() + 1);
		object->presenter_ = "MxWavePresenter";
		object->name_ = asset.name;
		object->filetype_ = si::MxOb::WAV;
		object->location_ = si::Vector3(0, 0, 0);
		object->direction_ = si::Vector3(0, 0, 1);
		object->up_ = si::Vector3(0, 1, 0);
		object->volume_ = 79;

		if (!object->ReplaceWithFile(file.c_str())) {
			abort();
		}

		si.AppendChild(object);
		depfile << result << ": " << (std::filesystem::current_path() / file).string() << std::endl;
		i++;
	}

	si.Write(result.c_str());
}

void CreateBadEnd()
{
	std::string result = out + "/badend.si";

	si::Interleaf si;
	mxHd.seek(0, si::MemoryBuffer::SeekStart);
	si.Read(&mxHd);

	si::Object* composite = new si::Object;
	std::string extra = "Replace:\\lego\\scripts\\intro:4";
	composite->id_ = 0;
	composite->type_ = si::MxOb::Presenter;
	composite->flags_ = MxDSAction::c_enabled;
	composite->duration_ = 0;
	composite->loops_ = 1;
	composite->extra_ = si::bytearray(extra.c_str(), extra.length() + 1);
	composite->presenter_ = "MxCompositeMediaPresenter";
	composite->name_ = "BadEnd_Movie";
	si.AppendChild(composite);

	si::Object* smk = new si::Object;
	std::string file = std::string("badend/BadEnd_Smk.smk");
	smk->id_ = 1;
	smk->type_ = si::MxOb::Video;
	smk->flags_ = MxDSAction::c_enabled;
	smk->duration_ = 75100;
	smk->loops_ = 1;
	smk->presenter_ = "MxSmkPresenter";
	smk->name_ = "BadEnd_Smk";
	smk->filetype_ = si::MxOb::SMK;
	smk->location_ = si::Vector3(0, 0, -10000);
	smk->direction_ = si::Vector3(0, 0, 1);
	smk->up_ = si::Vector3(0, 1, 0);
	smk->unknown29_ = 1; // Palette management = yes
	if (!smk->ReplaceWithFile(file.c_str())) {
		abort();
	}

	composite->AppendChild(smk);
	depfile << result << ": " << (std::filesystem::current_path() / file).string() << std::endl;

	si::Object* wav = new si::Object;
	file = std::string("badend/BadEnd_Wav.wav");
	wav->id_ = 2;
	wav->type_ = si::MxOb::Sound;
	wav->flags_ = MxDSAction::c_enabled;
	wav->duration_ = 77800;
	wav->loops_ = 1;
	wav->presenter_ = "MxWavePresenter";
	wav->name_ = "BadEnd_Wav";
	wav->filetype_ = si::MxOb::WAV;
	wav->location_ = si::Vector3(0, 0, 0);
	wav->direction_ = si::Vector3(0, 0, 1);
	wav->up_ = si::Vector3(0, 1, 0);
	wav->volume_ = 79;
	if (!wav->ReplaceWithFile(file.c_str())) {
		abort();
	}

	composite->AppendChild(wav);
	depfile << result << ": " << (std::filesystem::current_path() / file).string() << std::endl;

	si.Write(result.c_str());
}

void CreateRabbits()
{
	std::string result = out + "/rabbits.si";

	si::Interleaf si;
	mxHd.seek(0, si::MemoryBuffer::SeekStart);
	si.Read(&mxHd);

	// Object 0 (id=0): Blaubart model (fire-and-forget, loaded by chained Prepend)
	si::Object* blaubart = new si::Object;
	ZeroInitObject(blaubart);
	std::string file = std::string("rabbits/blaubart.mod");
	std::string extra = "Prepend:/lego/extra/rabbits;1";
	blaubart->id_ = 0;
	blaubart->type_ = si::MxOb::Object;
	blaubart->presenter_ = "LegoModelPresenter";
	blaubart->name_ = "blaubart";
	blaubart->flags_ = MxDSAction::c_enabled;
	blaubart->filetype_ = si::MxOb::OBJ;
	blaubart->loops_ = 1;
	blaubart->unknown28_ = 1;
	blaubart->unknown29_ = 1;
	blaubart->location_ = si::Vector3(-65.5f, 14.0f, 23.5f);
	blaubart->direction_ = si::Vector3(0, 0, 1);
	blaubart->up_ = si::Vector3(0, 1, 0);
	blaubart->extra_ = si::bytearray(extra.c_str(), extra.length() + 1);
	if (!blaubart->ReplaceWithFile(file.c_str())) {
		abort();
	}

	si.AppendChild(blaubart);
	depfile << result << ": " << (std::filesystem::current_path() / file).string() << std::endl;

	// Object 1 (id=1): Fluse model (fire-and-forget, loaded by chained Prepend from Blaubart)
	si::Object* fluse = new si::Object;
	ZeroInitObject(fluse);
	file = std::string("rabbits/fluse.mod");
	fluse->id_ = 1;
	fluse->type_ = si::MxOb::Object;
	fluse->presenter_ = "LegoModelPresenter";
	fluse->name_ = "fluse";
	fluse->flags_ = MxDSAction::c_enabled;
	fluse->filetype_ = si::MxOb::OBJ;
	fluse->loops_ = 1;
	fluse->unknown28_ = 1;
	fluse->unknown29_ = 1;
	fluse->location_ = si::Vector3(-62.0f, 14.0f, 28.0f);
	fluse->direction_ = si::Vector3(0, 0, 1);
	fluse->up_ = si::Vector3(0, 1, 0);
	if (!fluse->ReplaceWithFile(file.c_str())) {
		abort();
	}

	si.AppendChild(fluse);
	depfile << result << ": " << (std::filesystem::current_path() / file).string() << std::endl;

	// Object 2 (id=2): LegoAnimMMPresenter (composite)
	// EnableWith triggers when Isle world becomes active
	// Prepend chains: loads Blaubart model (id=0), which chains to Fluse model (id=1)
	si::Object* composite = new si::Object;
	ZeroInitObject(composite);
	extra = "EnableWith:\\Lego\\Scripts\\Isle\\Isle;0, Prepend:/lego/extra/rabbits;0";
	composite->id_ = 2;
	composite->type_ = si::MxOb::Presenter;
	composite->presenter_ = "LegoAnimMMPresenter";
	composite->flags_ = MxDSAction::c_enabled | MxDSAction::c_bit3;
	composite->loops_ = 1;
	composite->extra_ = si::bytearray(extra.c_str(), extra.length() + 1);
	composite->name_ = "rabbits_composite";
	composite->direction_ = si::Vector3(0, 0, 1);
	composite->up_ = si::Vector3(0, 1, 0);
	si.AppendChild(composite);

	// Child (id=3): Combined animation (both rabbits in one anim tree)
	si::Object* anim = new si::Object;
	ZeroInitObject(anim);
	file = std::string("rabbits/rabbits.ani");
	anim->id_ = 3;
	anim->type_ = si::MxOb::Object;
	anim->presenter_ = "LegoLoopingAnimPresenter";
	anim->name_ = "rabbits_anim";
	anim->flags_ = MxDSAction::c_enabled | si::MxOb::NoLoop;
	anim->filetype_ = si::MxOb::OBJ;
	anim->duration_ = -1;
	anim->loops_ = 1;
	anim->unknown28_ = 1;
	anim->unknown29_ = 1;
	anim->location_ = si::Vector3(0, 0, 0);
	anim->direction_ = si::Vector3(0, 0, 1);
	anim->up_ = si::Vector3(0, 1, 0);
	if (!anim->ReplaceWithFile(file.c_str())) {
		abort();
	}

	composite->AppendChild(anim);
	depfile << result << ": " << (std::filesystem::current_path() / file).string() << std::endl;

	si.Write(result.c_str());
}

struct HDAudioEntry {
	std::string si;
	uint32_t id;
	std::string wav;
};

std::vector<HDAudioEntry> ParseHDAudioIndex(const std::string& p_path)
{
	std::vector<HDAudioEntry> entries;
	std::ifstream in(p_path);
	if (!in) {
		abort();
	}

	std::string line;
	while (std::getline(in, line)) {
		if (line.empty() || line[0] == '#') {
			continue;
		}

		size_t a = line.find('\t');
		size_t b = line.find('\t', a + 1);
		if (a == std::string::npos || b == std::string::npos) {
			abort();
		}

		HDAudioEntry entry;
		entry.si = line.substr(0, a);
		entry.id = std::stoul(line.substr(a + 1, b - a - 1));
		entry.wav = line.substr(b + 1);
		entries.push_back(entry);
	}

	return entries;
}

void ReplaceWavAndFixup(si::Object* object, const std::string& file)
{
	if (object->data_.empty() || object->data_[0].size() != sizeof(si::WAVFmt)) {
		abort();
	}

	si::WAVFmt oldFmt = *object->data_[0].cast<si::WAVFmt>();
	uint64_t oldBody = object->GetFileBodySize();
	uint32_t oldPerLoop = (uint32_t) ((oldBody * 1000 + oldFmt.ByteRate - 1) / oldFmt.ByteRate);

	if (!object->ReplaceWithFile(file.c_str())) {
		abort();
	}

	// ReplaceWithFile stores the source fmt chunk verbatim; the engine expects
	// the 24-byte layout with DataSize (used to size cache sound buffers)
	if (object->data_[0].size() < 16) {
		abort();
	}

	si::WAVFmt fmt = *object->data_[0].cast<si::WAVFmt>();
	uint16_t blockAlign = fmt.Channels * (fmt.BitsPerSample / 8);
	uint32_t byteRate = fmt.SampleRate * blockAlign;
	uint64_t body = object->GetFileBodySize();

	si::bytearray header(sizeof(si::WAVFmt));
	si::WAVFmt* h = header.cast<si::WAVFmt>();
	h->Format = fmt.Format;
	h->Channels = fmt.Channels;
	h->SampleRate = fmt.SampleRate;
	h->ByteRate = byteRate;
	h->BlockAlign = blockAlign;
	h->BitsPerSample = fmt.BitsPerSample;
	h->DataSize = (uint32_t) body;
	h->Flags = oldFmt.Flags;
	object->data_[0] = header;

	uint32_t loops = object->loops_ ? object->loops_ : 1;
	uint32_t perLoop = (uint32_t) ((body * 1000 + byteRate - 1) / byteRate);

	// Recompute audio-driven durations; padded durations are kept unless the
	// new audio outgrows them (looping buffers are sized from duration/loops)
	if (object->duration_ != (uint32_t) -1 &&
		(object->duration_ <= (oldPerLoop + 50) * loops || object->duration_ < perLoop * loops)) {
		object->duration_ = perLoop * loops;
	}
}

void CreateHDAudio()
{
	std::vector<HDAudioEntry> entries = ParseHDAudioIndex("hdaudio/index.txt");
	std::map<std::string, std::vector<const HDAudioEntry*>> bySi;
	for (const HDAudioEntry& entry : entries) {
		bySi[entry.si].push_back(&entry);
	}

	for (const auto& pair : bySi) {
		std::string orig = "hdaudio/orig/" + pair.first;

		char magic[4] = {};
		std::ifstream check(orig, std::ios::binary);
		check.read(magic, sizeof(magic));
		if (memcmp(magic, "RIFF", sizeof(magic)) != 0) {
			fprintf(stderr, "%s is not an SI file (missing `git lfs pull`?)\n", orig.c_str());
			abort();
		}

		si::Interleaf si;
		if (si.Read(orig.c_str(), si::Interleaf::IncludeData) != si::Interleaf::ERROR_SUCCESS) {
			abort();
		}

		std::string result = out + "/hdaudio/LEGO/Scripts/" + pair.first;

		for (const HDAudioEntry* entry : pair.second) {
			si::Object* object = NULL;
			for (size_t i = 0; i < si.GetChildCount() && !object; i++) {
				si::Object* child = static_cast<si::Object*>(si.GetChildAt(i));
				if (child->type_ == si::MxOb::Null) {
					continue;
				}
				object = child->FindSubObjectWithID(entry->id);
			}

			if (!object || object->filetype_ != si::MxOb::WAV) {
				abort();
			}

			std::string file = "hdaudio/" + entry->wav;
			ReplaceWavAndFixup(object, file);
			depfile << result << ": " << (std::filesystem::current_path() / file).string() << std::endl;
		}

		std::filesystem::create_directories(std::filesystem::path(result).parent_path());
		std::filesystem::remove(result);
		if (si.Write(result.c_str()) != si::Interleaf::ERROR_SUCCESS) {
			abort();
		}

		depfile << result << ": " << (std::filesystem::current_path() / orig).string() << std::endl;
		depfile << result << ": " << (std::filesystem::current_path() / "hdaudio/index.txt").string() << std::endl;
	}
}

int main(int argc, char* argv[])
{
	out = argv[1];
	depfile = std::ofstream(argv[2]);

	mxHd.WriteU32(si::RIFF::MxHd);
	mxHd.WriteU32(3 * sizeof(uint32_t));
	mxHd.WriteU32(version);
	mxHd.WriteU32(bufferSize);
	mxHd.WriteU32(bufferCount);

	CreateWidescreen();
	CreateHDMusic();
	CreateBadEnd();
	CreateRabbits();
	CreateHDAudio();
	return 0;
}
