#include "filesystem.h"

#include "legogamestate.h"
#include "misc.h"
#include "mxomni.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_log.h>
#include <emscripten.h>
#include <emscripten/wasmfs.h>

static backend_t opfs = nullptr;
static backend_t fetchfs = nullptr;

extern const char* g_files[46];

bool Emscripten_OPFSDisabled()
{
	return MAIN_THREAD_EM_ASM_INT({return !!Module["disableOpfs"]});
}

bool Emscripten_SetupConfig(const char* p_iniConfig)
{
	if (Emscripten_OPFSDisabled()) {
		SDL_Log("OPFS is disabled; ignoring .ini path");
		return false;
	}

	opfs = wasmfs_create_opfs_backend();
	MxString iniConfig = p_iniConfig;

	char* parse = iniConfig.GetData();
	while ((parse = SDL_strchr(++parse, '/'))) {
		*parse = '\0';
		wasmfs_create_directory(iniConfig.GetData(), 0644, opfs);
		*parse = '/';
	}

	return true;
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

	for (const char* file : g_files) {
		registerFile(file);
	}

	if (GameState()->GetSavePath() && *GameState()->GetSavePath() && !Emscripten_OPFSDisabled()) {
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
