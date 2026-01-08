#ifdef MORTAR_DLL
#ifdef BUILDING_MORTAR
#ifdef _WIN32
#define MORTAR_DECLSPEC __declspec(dllexport)
#else
#define MORTAR_DECLSPEC __attribute__((visibility("default")))
#endif
#else
#ifdef _WIN32
#define MORTAR_DECLSPEC __declspec(dllimport)
#else
#define MORTAR_DECLSPEC
#endif
#endif
#else
#define MORTAR_DECLSPEC
#endif

#ifdef __cplusplus
extern "C"
{
#endif
