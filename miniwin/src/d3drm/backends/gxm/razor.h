#pragma once

#include <psp2/razor_capture.h>
#include <psp2/razor_hud.h>

extern "C"
{
	extern int sceRazorGpuCaptureSetTrigger(int frames, const char* path);

	extern int sceRazorGpuTraceTrigger();
	extern int sceRazorGpuTraceSetFilename(const char* filename, int counter);
	extern int sceRazorHudSetDisplayEnabled(bool enable);
}
