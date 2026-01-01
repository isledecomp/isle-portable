#pragma once

#include "mortar_begin.h"

#include <stdarg.h>

enum {
	MORTAR_LOG_CATEGORY_APPLICATION,
	MORTAR_LOG_CATEGORY_CUSTOM
};

typedef enum MORTAR_LogPriority {
	MORTAR_LOG_PRIORITY_INVALID,
	MORTAR_LOG_PRIORITY_TRACE,
	MORTAR_LOG_PRIORITY_VERBOSE,
	MORTAR_LOG_PRIORITY_DEBUG,
	MORTAR_LOG_PRIORITY_INFO,
	MORTAR_LOG_PRIORITY_WARN,
	MORTAR_LOG_PRIORITY_ERROR,
	MORTAR_LOG_PRIORITY_CRITICAL
} MORTAR_LogPriority;

extern MORTAR_DECLSPEC void MORTAR_LogError(int category, const char* fmt, ...);

extern MORTAR_DECLSPEC void MORTAR_LogWarn(int category, const char* fmt, ...);

extern MORTAR_DECLSPEC void MORTAR_LogTrace(int category, const char* fmt, ...);

extern MORTAR_DECLSPEC void MORTAR_LogInfo(int category, const char* fmt, ...);

extern MORTAR_DECLSPEC void MORTAR_Log(const char* fmt, ...);

extern MORTAR_DECLSPEC void MORTAR_LogMessageV(int category, MORTAR_LogPriority priority, const char* fmt, va_list ap);

#include "mortar_end.h"
