
#include "mxdsaction.h"

#include <file.h>
#include <interleaf.h>
#include <object.h>

si::Interleaf::Version version = si::Interleaf::Version2_2;
uint32_t bufferSize = 65535;
uint32_t bufferCount = 8;

std::string out;
si::MemoryBuffer mxHd;

void CreateWidescreen()
{
	si::Interleaf si;
	mxHd.seek(0, si::MemoryBuffer::SeekStart);
	si.Read(&mxHd);

	si::Object GaraDoor_Wide;
	const char extra[] =
		"World:current, StartWith:\\Lego\\Scripts\\Isle\\Isle;1160, RemoveWith:\\Lego\\Scripts\\Isle\\Isle;1161";
	GaraDoor_Wide.type_ = si::MxOb::Bitmap;
	GaraDoor_Wide.flags_ = MxDSAction::c_enabled | MxDSAction::c_bit4;
	GaraDoor_Wide.duration_ = -1;
	GaraDoor_Wide.loops_ = 1;
	GaraDoor_Wide.extra_ = si::bytearray(extra, sizeof(extra));
	GaraDoor_Wide.presenter_ = "MxStillPresenter";
	GaraDoor_Wide.name_ = "GaraDoor_Wide";
	GaraDoor_Wide.filetype_ = si::MxOb::STL;
	GaraDoor_Wide.location_.x = -240.0;
	GaraDoor_Wide.location_.z = -1.0;
	GaraDoor_Wide.up_.y = 1.0;
	GaraDoor_Wide.ReplaceWithFile("widescreen/garadoor.bmp");
	si.AppendChild(&GaraDoor_Wide);

	si.Write((out + "/widescreen.si").c_str());
}

int main(int argc, char* argv[])
{
	out = argv[1];

	mxHd.WriteU32(si::RIFF::MxHd);
	mxHd.WriteU32(3 * sizeof(uint32_t));
	mxHd.WriteU32(version);
	mxHd.WriteU32(bufferSize);
	mxHd.WriteU32(bufferCount);

	CreateWidescreen();
	return 0;
}
