#ifndef LEGO1_EXPORT_H
#define LEGO1_EXPORT_H

#ifdef LEGO1_STATIC
#define LEGO1_EXPORT
#elif defined(LEGO1_DLL)
#ifdef _WIN32
#define LEGO1_EXPORT __declspec(dllexport)
#else
#define LEGO1_EXPORT __attribute__((visibility("default")))
#endif
#else
#ifdef _WIN32
#define LEGO1_EXPORT __declspec(dllimport)
#else
#define LEGO1_EXPORT
#endif
#endif

#endif
