#pragma once

#include "mortar_begin.h"

#include <stddef.h>
#include <stdint.h>

#define MORTAR_ENUM_FLAG_OPERATORS(ENUMTYPE)                                                                           \
	inline ENUMTYPE operator|(ENUMTYPE a, ENUMTYPE b)                                                                  \
	{                                                                                                                  \
		return ENUMTYPE(((int) a) | ((int) b));                                                                        \
	}                                                                                                                  \
	inline ENUMTYPE& operator|=(ENUMTYPE& a, ENUMTYPE b)                                                               \
	{                                                                                                                  \
		return (ENUMTYPE&) (((int&) a) |= ((int) b));                                                                  \
	}                                                                                                                  \
	inline ENUMTYPE operator&(ENUMTYPE a, ENUMTYPE b)                                                                  \
	{                                                                                                                  \
		return ENUMTYPE(((int) a) & ((int) b));                                                                        \
	}                                                                                                                  \
	inline ENUMTYPE& operator&=(ENUMTYPE& a, ENUMTYPE b)                                                               \
	{                                                                                                                  \
		return (ENUMTYPE&) (((int&) a) &= ((int) b));                                                                  \
	}                                                                                                                  \
	inline ENUMTYPE operator~(ENUMTYPE a)                                                                              \
	{                                                                                                                  \
		return ENUMTYPE(~((int) a));                                                                                   \
	}                                                                                                                  \
	inline ENUMTYPE operator^(ENUMTYPE a, ENUMTYPE b)                                                                  \
	{                                                                                                                  \
		return ENUMTYPE(((int) a) ^ ((int) b));                                                                        \
	}                                                                                                                  \
	inline ENUMTYPE& operator^=(ENUMTYPE& a, ENUMTYPE b)                                                               \
	{                                                                                                                  \
		return (ENUMTYPE&) (((int&) a) ^= ((int) b));                                                                  \
	}

#define MORTAR_zero(x) MORTAR_memset(&(x), 0, sizeof((x)))

#define MORTAR_clamp(x, a, b) (((x) < (a)) ? (a) : (((x) > (b)) ? (b) : (x)))

#define MORTAR_max(x, y) (((x) > (y)) ? (x) : (y))

typedef void (*MORTAR_FunctionPointer)(void);

typedef struct MORTAR_GLContextState* MORTAR_GLContext;

extern MORTAR_DECLSPEC void* MORTAR_malloc(size_t size);

extern MORTAR_DECLSPEC void MORTAR_free(void* ptr);

extern MORTAR_DECLSPEC char* MORTAR_itoa(int value, char* str, int radix);

extern MORTAR_DECLSPEC void* MORTAR_memset(void* dst, int c, size_t len);

extern MORTAR_DECLSPEC int MORTAR_memcmp(const void* s1, const void* s2, size_t len);

extern MORTAR_DECLSPEC int MORTAR_strcasecmp(const char* str1, const char* str2);

extern MORTAR_DECLSPEC int MORTAR_strncasecmp(const char* str1, const char* str2, size_t maxlen);

extern MORTAR_DECLSPEC size_t MORTAR_strlen(const char* str);

extern MORTAR_DECLSPEC char* MORTAR_strdup(const char* s);

size_t MORTAR_DECLSPEC MORTAR_strlcpy(char* dst, const char* src, size_t maxlen);

extern MORTAR_DECLSPEC char* MORTAR_strlwr(char* str);

extern MORTAR_DECLSPEC char* MORTAR_strupr(char* str);

extern MORTAR_DECLSPEC int MORTAR_strncmp(const char* str1, const char* str2, size_t maxlen);

extern MORTAR_DECLSPEC const char* MORTAR_strstr(const char* haystack, const char* needle);

extern MORTAR_DECLSPEC char* MORTAR_strtok_r(char* str, const char* delim, char** saveptr);

extern MORTAR_DECLSPEC char* MORTAR_strchr(const char* str, int c);

extern MORTAR_DECLSPEC int MORTAR_sscanf(const char* text, const char* fmt, ...);

extern MORTAR_DECLSPEC int MORTAR_snprintf(char* text, size_t maxlen, const char* fmt, ...);

extern MORTAR_DECLSPEC int MORTAR_tolower(int x);

extern MORTAR_DECLSPEC int MORTAR_isdigit(int x);

extern MORTAR_DECLSPEC int32_t MORTAR_rand(int32_t n);

extern MORTAR_DECLSPEC float MORTAR_randf(void);

#include "mortar_end.h"
