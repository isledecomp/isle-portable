#ifndef COMPAT_H
#define COMPAT_H

// Various macros to enable compiling with other/newer compilers.

#if defined(__MINGW32__) || (defined(_MSC_VER) && _MSC_VER >= 1100) || !defined(_WIN32)
#define COMPAT_MODE
#endif

#ifndef MINIWIN
#define D3DCOLOR_NONE 0
#define D3DRMMAP_NONE 0
#define PC_NONE 0
#define DDBLT_NONE 0
#define D3DRMRENDERMODE DWORD
#define DDSCapsFlags DWORD
#define DDBitDepths DWORD
#endif

// SDL will not put the message box on the main thread by default.
// See: https://github.com/libsdl-org/SDL/issues/12943
#ifdef __EMSCRIPTEN__
#define Any_ShowSimpleMessageBox Emscripten_ShowSimpleMessageBox
#elif defined(__vita__)
#define Any_ShowSimpleMessageBox Vita_ShowSimpleMessageBox
#else
#define Any_ShowSimpleMessageBox MORTAR_ShowSimpleMessageBox
#endif

// Disable "identifier was truncated to '255' characters" warning.
// Impossible to avoid this if using STL map or set.
// This removes most (but not all) occurrences of the warning.
#pragma warning(disable : 4786)

#define MSVC420_VERSION 1020

// We use `override` so newer compilers can tell us our vtables are valid,
// however this keyword was added in C++11, so we define it as empty for
// compatibility with older compilers.
#if __cplusplus < 201103L
#define override
#define static_assert(expr, msg)
#endif

#endif // COMPAT_H
