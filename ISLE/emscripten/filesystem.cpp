#include "filesystem.h"

#include "legogamestate.h"
#include "misc.h"
#include "mxomni.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_log.h>
#include <emscripten/wasmfs.h>

static backend_t opfs = nullptr;
static backend_t fetchfs = nullptr;

void Emscripten_SetupConfig(const char* p_iniConfig)
{
	if (!p_iniConfig || !*p_iniConfig) {
		return;
	}

	opfs = wasmfs_create_opfs_backend();
	MxString iniConfig = p_iniConfig;

	char* parse = iniConfig.GetData();
	while ((parse = SDL_strchr(++parse, '/'))) {
		*parse = '\0';
		wasmfs_create_directory(iniConfig.GetData(), 0644, opfs);
		*parse = '/';
	}
}

void Emscripten_SetupFilesystem()
{
	fetchfs = wasmfs_create_fetch_backend((MxString(Emscripten_streamHost) + MxString("/LEGO")).GetData(), 512 * 1024);

	wasmfs_create_directory("/LEGO", 0644, fetchfs);
	wasmfs_create_directory("/LEGO/Scripts", 0644, fetchfs);
	wasmfs_create_directory("/LEGO/Scripts/Act2", 0644, fetchfs);
	wasmfs_create_directory("/LEGO/Scripts/Act3", 0644, fetchfs);
	wasmfs_create_directory("/LEGO/Scripts/Build", 0644, fetchfs);
	wasmfs_create_directory("/LEGO/Scripts/Garage", 0644, fetchfs);
	wasmfs_create_directory("/LEGO/Scripts/Hospital", 0644, fetchfs);
	wasmfs_create_directory("/LEGO/Scripts/Infocntr", 0644, fetchfs);
	wasmfs_create_directory("/LEGO/Scripts/Isle", 0644, fetchfs);
	wasmfs_create_directory("/LEGO/Scripts/Police", 0644, fetchfs);
	wasmfs_create_directory("/LEGO/Scripts/Race", 0644, fetchfs);
	wasmfs_create_directory("/LEGO/data", 0644, fetchfs);

	const auto registerFile = [](const char* p_path) {
		MxString path = MxString(Emscripten_bundledPath) + MxString(p_path);
		path.MapPathToFilesystem();

		if (SDL_GetPathInfo(path.GetData(), NULL)) {
			SDL_Log("File %s is bundled and won't be streamed", p_path);
		}
		else {
			wasmfs_create_file(p_path, 0644, fetchfs);
			MxOmni::GetCDFiles().emplace_back(p_path);

			SDL_Log("File %s set up for streaming", p_path);
		}
	};

	registerFile("/LEGO/Scripts/CREDITS.SI");
	registerFile("/LEGO/Scripts/INTRO.SI");
	registerFile("/LEGO/Scripts/NOCD.SI");
	registerFile("/LEGO/Scripts/SNDANIM.SI");
	registerFile("/LEGO/Scripts/Act2/ACT2MAIN.SI");
	registerFile("/LEGO/Scripts/Act3/ACT3.SI");
	registerFile("/LEGO/Scripts/Build/COPTER.SI");
	registerFile("/LEGO/Scripts/Build/DUNECAR.SI");
	registerFile("/LEGO/Scripts/Build/JETSKI.SI");
	registerFile("/LEGO/Scripts/Build/RACECAR.SI");
	registerFile("/LEGO/Scripts/Garage/GARAGE.SI");
	registerFile("/LEGO/Scripts/Hospital/HOSPITAL.SI");
	registerFile("/LEGO/Scripts/Infocntr/ELEVBOTT.SI");
	registerFile("/LEGO/Scripts/Infocntr/HISTBOOK.SI");
	registerFile("/LEGO/Scripts/Infocntr/INFODOOR.SI");
	registerFile("/LEGO/Scripts/Infocntr/INFOMAIN.SI");
	registerFile("/LEGO/Scripts/Infocntr/INFOSCOR.SI");
	registerFile("/LEGO/Scripts/Infocntr/REGBOOK.SI");
	registerFile("/LEGO/Scripts/Isle/ISLE.SI");
	registerFile("/LEGO/Scripts/Isle/JUKEBOX.SI");
	registerFile("/LEGO/Scripts/Isle/JUKEBOXW.SI");
	registerFile("/LEGO/Scripts/Police/POLICE.SI");
	registerFile("/LEGO/Scripts/Race/CARRACE.SI");
	registerFile("/LEGO/Scripts/Race/CARRACER.SI");
	registerFile("/LEGO/Scripts/Race/JETRACE.SI");
	registerFile("/LEGO/Scripts/Race/JETRACER.SI");
	registerFile("/LEGO/data/ACT1INF.DTA");
	registerFile("/LEGO/data/ACT2INF.DTA");
	registerFile("/LEGO/data/ACT3INF.DTA");
	registerFile("/LEGO/data/BLDDINF.DTA");
	registerFile("/LEGO/data/BLDHINF.DTA");
	registerFile("/LEGO/data/BLDJINF.DTA");
	registerFile("/LEGO/data/BLDRINF.DTA");
	registerFile("/LEGO/data/GMAININF.DTA");
	registerFile("/LEGO/data/HOSPINF.DTA");
	registerFile("/LEGO/data/ICUBEINF.DTA");
	registerFile("/LEGO/data/IELEVINF.DTA");
	registerFile("/LEGO/data/IISLEINF.DTA");
	registerFile("/LEGO/data/IMAININF.DTA");
	registerFile("/LEGO/data/IREGINF.DTA");
	registerFile("/LEGO/data/OBSTINF.DTA");
	registerFile("/LEGO/data/PMAININF.DTA");
	registerFile("/LEGO/data/RACCINF.DTA");
	registerFile("/LEGO/data/RACJINF.DTA");
	registerFile("/LEGO/data/WORLD.WDB");
	registerFile("/LEGO/data/testinf.dta");

	if (GameState()->GetSavePath() && *GameState()->GetSavePath()) {
		if (!opfs) {
			opfs = wasmfs_create_opfs_backend();
		}

		MxString savePath = GameState()->GetSavePath();
		if (savePath.GetData()[savePath.GetLength() - 1] != '/') {
			savePath += "/";
		}

		char* parse = savePath.GetData();
		while ((parse = SDL_strchr(++parse, '/'))) {
			*parse = '\0';
			wasmfs_create_directory(savePath.GetData(), 0644, opfs);
			*parse = '/';
		}
	}
}
