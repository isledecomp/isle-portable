#include "mortar/mortar_stdinc.h"
#include "sdl3_internal.h"

void* MORTAR_malloc(size_t size)
{
	return SDL_malloc(size);
}

void MORTAR_free(void* ptr)
{
	SDL_free(ptr);
}

char* MORTAR_itoa(int value, char* str, int radix)
{
	return SDL_itoa(value, str, radix);
}

void* MORTAR_memset(void* dst, int c, size_t len)
{
	return SDL_memset(dst, c, len);
}

int MORTAR_memcmp(const void* s1, const void* s2, size_t len)
{
	return SDL_memcmp(s1, s2, len);
}

int MORTAR_strcasecmp(const char* str1, const char* str2)
{
	return SDL_strcasecmp(str1, str2);
}

int MORTAR_strncasecmp(const char* str1, const char* str2, size_t maxlen)
{
	return SDL_strncasecmp(str1, str2, maxlen);
}

size_t MORTAR_strlen(const char* str)
{
	return SDL_strlen(str);
}

char* MORTAR_strdup(const char* s)
{
	return SDL_strdup(s);
}

size_t MORTAR_strlcpy(char* dst, const char* src, size_t maxlen)
{
	return SDL_strlcpy(dst, src, maxlen);
}

char* MORTAR_strlwr(char* str)
{
	return SDL_strlwr(str);
}

char* MORTAR_strupr(char* str)
{
	return SDL_strupr(str);
}

int MORTAR_strncmp(const char* str1, const char* str2, size_t maxlen)
{
	return SDL_strncmp(str1, str2, maxlen);
}

const char* MORTAR_strstr(const char* haystack, const char* needle)
{
	return SDL_strstr(haystack, needle);
}

char* MORTAR_strtok_r(char* str, const char* delim, char** saveptr)
{
	return SDL_strtok_r(str, delim, saveptr);
}

char* MORTAR_strchr(const char* str, int c)
{
	return (char*) SDL_strchr(str, c);
}

int MORTAR_sscanf(const char* text, const char* fmt, ...)
{
	int result;
	va_list ap;
	va_start(ap, fmt);
	result = SDL_vsscanf(text, fmt, ap);
	va_end(ap);
	return result;
}

int MORTAR_snprintf(char* text, size_t maxlen, const char* fmt, ...)
{
	int result;
	va_list ap;
	va_start(ap, fmt);
	result = SDL_vsnprintf(text, maxlen, fmt, ap);
	va_end(ap);
	return result;
}

int MORTAR_tolower(int x)
{
	return SDL_tolower(x);
}

int MORTAR_isdigit(int x)
{
	return SDL_isdigit(x);
}

int32_t MORTAR_rand(int32_t n)
{
	return SDL_rand(n);
}

float MORTAR_randf()
{
	return SDL_randf();
}
