#include "mxdsaction.h"

#include <file.h>
#include <filesystem>
#include <fstream>
#include <interleaf.h>
#include <object.h>
#include <string_view>

struct AssetView {
	std::string_view name;
	std::string_view extra;
};

si::Interleaf::Version version = si::Interleaf::Version2_2;
uint32_t bufferSize = 65536;
uint32_t bufferCount = 8;

std::string out;
std::ofstream depfile;
si::MemoryBuffer mxHd;

void CreateWidescreen()
{
	std::string result = out + "/widescreen.si";
	constexpr const AssetView widescreenBitmaps[] = {
		{"GaraDoor_Background_Wide",
		 "World:current, StartWith:\\Lego\\Scripts\\Isle\\Isle;1160, RemoveWith:\\Lego\\Scripts\\Isle\\Isle;1161"}
	};

	si::Interleaf si;
	mxHd.seek(0, si::MemoryBuffer::SeekStart);
	si.Read(&mxHd);

	int i = 0;
	for (const AssetView& asset : widescreenBitmaps) {
		si::Object* object = new si::Object;
		std::string file = std::string("widescreen/") + std::string(asset.name) + ".bmp";
		object->id_ = i;
		object->type_ = si::MxOb::Bitmap;
		object->flags_ = MxDSAction::c_enabled | MxDSAction::c_bit4;
		object->duration_ = -1;
		object->loops_ = 1;
		object->extra_ = si::bytearray(asset.extra.data(), asset.extra.length());
		object->presenter_ = "MxStillPresenter";
		object->name_ = asset.name;
		object->filetype_ = si::MxOb::STL;
		object->location_ = si::Vector3(-240.0, 0.0, -1.0);
		object->direction_ = si::Vector3(0, 0, 0);
		object->up_ = si::Vector3(0, 1.0, 0);
		object->ReplaceWithFile(file.c_str());
		si.AppendChild(object);
		depfile << result << ": " << (std::filesystem::current_path() / file).string() << std::endl;
		i++;
	}

	si.Write(result.c_str());
}

void CreateHDMusic()
{
	std::string result = out + "/hdmusic.si";
	constexpr const AssetView wavAudio[] = {{"JBMusic1_HD", "Replace:\\Lego\\Scripts\\Isle\\Jukebox;55"}};

	si::Interleaf si;
	mxHd.seek(0, si::MemoryBuffer::SeekStart);
	si.Read(&mxHd);

	int i = 0;
	for (const AssetView& asset : wavAudio) {
		si::Object* object = new si::Object;
		std::string file = std::string("hdmusic/") + std::string(asset.name) + ".wav";
		object->id_ = i;
		object->type_ = si::MxOb::Sound;
		object->flags_ = MxDSAction::c_enabled;
		object->duration_ = 125850;
		object->loops_ = 1;
		object->extra_ = si::bytearray(asset.extra.data(), asset.extra.length());
		object->presenter_ = "MxWavePresenter";
		object->name_ = asset.name;
		object->filetype_ = si::MxOb::WAV;
		object->location_ = si::Vector3(0, 0, 0);
		object->direction_ = si::Vector3(0, 0, 0);
		object->up_ = si::Vector3(0, 1.0, 0);
		object->volume_ = 79;
		object->ReplaceWithFile(file.c_str());
		si.AppendChild(object);
		depfile << result << ": " << (std::filesystem::current_path() / file).string() << std::endl;
		i++;
	}

	si.Write(result.c_str());
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
