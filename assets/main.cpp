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
	si::Interleaf si;
	mxHd.seek(0, si::MemoryBuffer::SeekStart);
	si.Read(&mxHd);

	si::Object GaraDoor_Background_Wide;
	const char extra[] =
		"World:current, StartWith:\\Lego\\Scripts\\Isle\\Isle;1160, RemoveWith:\\Lego\\Scripts\\Isle\\Isle;1161";
	GaraDoor_Background_Wide.type_ = si::MxOb::Bitmap;
	GaraDoor_Background_Wide.flags_ = MxDSAction::c_enabled | MxDSAction::c_bit4;
	GaraDoor_Background_Wide.duration_ = -1;
	GaraDoor_Background_Wide.loops_ = 1;
	GaraDoor_Background_Wide.extra_ = si::bytearray(extra, sizeof(extra));
	GaraDoor_Background_Wide.presenter_ = "MxStillPresenter";
	GaraDoor_Background_Wide.name_ = "GaraDoor_Background_Wide";
	GaraDoor_Background_Wide.filetype_ = si::MxOb::STL;
	GaraDoor_Background_Wide.location_ = si::Vector3(-240.0, 0.0, -1.0);
	GaraDoor_Background_Wide.direction_ = si::Vector3(0, 0, 0);
	GaraDoor_Background_Wide.up_ = si::Vector3(0, 1.0, 0);
	GaraDoor_Background_Wide.ReplaceWithFile("widescreen/GaraDoor_Background_Wide.bmp");
	si.AppendChild(&GaraDoor_Background_Wide);

	std::string file = out + "/widescreen.si";
	depfile << file << ": " << (std::filesystem::current_path() / "widescreen/GaraDoor_Background_Wide.bmp").string()
			<< std::endl;

	si.Write(file.c_str());
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
	return 0;
}
