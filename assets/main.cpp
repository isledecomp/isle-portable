#include "mxdsaction.h"

#include <file.h>
#include <filesystem>
#include <fstream>
#include <interleaf.h>
#include <object.h>

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
	};
	const AssetView widescreenBitmaps[] = {
		{"GaraDoor_Background_Wide",
		 "World:current, StartWith:\\Lego\\Scripts\\Isle\\Isle;1160, RemoveWith:\\Lego\\Scripts\\Isle\\Isle;1161"}
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
		object->location_ = si::Vector3(-240.0, 0.0, -1.0);
		object->direction_ = si::Vector3(0, 0, 0);
		object->up_ = si::Vector3(0, 1.0, 0);

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
		 152600,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3},
		{"Cave_Music_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;7",
		 69240,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3},
		{"CentralRoads_Music_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;8",
		 193380,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3},
		{"Jail_Music_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;9",
		 68820,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3},
		{"Hospital_Music_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;10",
		 211990,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3},
		{"InformationCenter_Music_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;11",
		 154510,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3},
		{"PoliceStation_Music_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;12",
		 57090,
		 10000,
		 MxDSAction::c_enabled | MxDSAction::c_bit3},
		{"Park_Music_HD",
		 "Replace:\\Lego\\Scripts\\Isle\\Jukebox;13",
		 91210,
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
	smk->id_ = 1;
	smk->type_ = si::MxOb::SMK;
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
	return 0;
}
