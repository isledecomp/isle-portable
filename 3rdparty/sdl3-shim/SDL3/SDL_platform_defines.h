#pragma once

#include <SDL2/SDL_platform.h>

// https://wiki.libsdl.org/SDL3/README-migration#sdl_platformh

// #define SDL_PLATFORM_3DS __3DS__
// #define SDL_PLATFORM_AIX __AIX__
// #define SDL_PLATFORM_ANDROID __ANDROID__
// #define SDL_PLATFORM_APPLE __APPLE__
// #define SDL_PLATFORM_BSDI __BSDI__
#ifdef __CYGWIN__
#define SDL_PLATFORM_CYGWIN 1
#endif
// #define SDL_PLATFORM_EMSCRIPTEN __EMSCRIPTEN__
// #define SDL_PLATFORM_FREEBSD __FREEBSD__
// #define SDL_PLATFORM_GDK __GDK__
// #define SDL_PLATFORM_HAIKU __HAIKU__
// #define SDL_PLATFORM_HPUX __HPUX__
// #define SDL_PLATFORM_IOS __IPHONEOS__
// #define SDL_PLATFORM_IRIX __IRIX__
// #define SDL_PLATFORM_LINUX __LINUX__
// #define SDL_PLATFORM_MACOS __MACOSX__
// #define SDL_PLATFORM_NETBSD __NETBSD__
// #define SDL_PLATFORM_OPENBSD __OPENBSD__
// #define SDL_PLATFORM_OS2 __OS2__
// #define SDL_PLATFORM_OSF __OSF__
// #define SDL_PLATFORM_PS2 __PS2__
// #define SDL_PLATFORM_PSP __PSP__
// #define SDL_PLATFORM_QNXNTO __QNXNTO__
// #define SDL_PLATFORM_RISCOS __RISCOS__
// #define SDL_PLATFORM_SOLARIS __SOLARIS__
// #define SDL_PLATFORM_TVOS __TVOS__
// #define SDL_PLATFORM_UNI __unix__
// #define SDL_PLATFORM_VITA __VITA__
// #define SDL_PLATFORM_WIN32 __WIN32__
// #define SDL_PLATFORM_WINGDK __WINGDK__
// #define SDL_PLATFORM_XBOXONE __XBOXONE__
// #define SDL_PLATFORM_XBOXSERIES __XBOXSERIES__

#if defined(_WIN32) || defined(SDL_PLATFORM_CYGWIN)
#define SDL_PLATFORM_WINDOWS 1
#endif
