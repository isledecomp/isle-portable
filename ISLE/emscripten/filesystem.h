#ifndef EMSCRIPTEN_FILESYSTEM_H
#define EMSCRIPTEN_FILESYSTEM_H

#ifndef ISLE_EMSCRIPTEN_HOST
#define ISLE_EMSCRIPTEN_HOST ""
#endif

inline static const char* Emscripten_bundledPath = "/bundled";
inline static const char* Emscripten_savePath = "/save";
inline static const char* Emscripten_streamPath = "/";
inline static const char* Emscripten_streamHost = ISLE_EMSCRIPTEN_HOST;

bool Emscripten_SetupConfig(const char* p_iniConfig);
void Emscripten_SetupFilesystem();

#endif // EMSCRIPTEN_FILESYSTEM_H
