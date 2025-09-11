#ifndef __PAFSTD__
#define __PAFSTD__

#include <paf/std/stdc.h>
#include <inttypes.h>

#define malloc sce_paf_malloc
#define memcpy sce_paf_memcpy
#define memset sce_paf_memset
#define memmove sce_paf_memmove
#define free sce_paf_free
#define calloc sce_paf_calloc
#define strcmp sce_paf_strcmp
#define strlen sce_paf_strlen
#define strncmp sce_paf_strncmp
#define strchr sce_paf_strchr
#define strtol sce_paf_strtol
#define atof sce_paf_atof

// _ctype_ isnt called that in scelibc, so just stub the functions that are needed

inline int _is_space(char c)
{
	return (c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v');
}

inline int _to_lower(int c)
{
	if (c >= 'A' && c <= 'Z') {
		return c + ('a' - 'A');
	}
	return c;
}

inline int _is_digit(int c)
{
	return (c >= '0' && c <= '9');
}

#undef isspace
#undef tolower
#undef isdigit
#define isspace _is_space
#define tolower _to_lower
#define isdigit _is_digit

#endif
