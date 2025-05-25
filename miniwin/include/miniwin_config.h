#ifndef MINIWIN_CONFIG_H
#define MINIWIN_CONFIG_H

#include <string>

#ifdef _WIN32
#define MINIWIN_EXPORT __declspec(dllexport)
#else
#define MINIWIN_EXPORT __attribute__((visibility("default")))
#endif

enum MiniwinBackendType {
	eInvalid,
	eSDL3GPU,
};

MiniwinBackendType MINIWIN_EXPORT Miniwin_StringToBackendType(const char* str);

std::string MINIWIN_EXPORT Miniwin_BackendTypeToString(MiniwinBackendType type);

void MINIWIN_EXPORT Miniwin_ConfigureBackend(MiniwinBackendType type);

#endif
