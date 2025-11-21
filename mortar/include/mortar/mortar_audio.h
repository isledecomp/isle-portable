#pragma once

#include "mortar_begin.h"

#include <stdint.h>

typedef struct MORTAR_AudioStream MORTAR_AudioStream;

typedef enum MORTAR_AudioFormat {
	MORTAR_AUDIO_UNKNOWN = 0,
	MORTAR_AUDIO_F32,
} MORTAR_AudioFormat;

typedef struct MORTAR_AudioSpec {
	MORTAR_AudioFormat format;
	int channels;
	int freq;
} MORTAR_AudioSpec;

typedef uint32_t MORTAR_AudioDeviceID;

typedef void (*MORTAR_AudioStreamCallback)(
	void* userdata,
	MORTAR_AudioStream* stream,
	int additional_amount,
	int total_amount
);

extern MORTAR_DECLSPEC MORTAR_AudioStream* MORTAR_EX_OpenAudioPlaybackDevice(
	const MORTAR_AudioSpec* spec,
	MORTAR_AudioStreamCallback callback,
	void* userdata
);

extern MORTAR_DECLSPEC void MORTAR_DestroyAudioStream(MORTAR_AudioStream* stream);

extern MORTAR_DECLSPEC bool MORTAR_EX_ResumeAudioDevice(MORTAR_AudioStream* stream);

extern MORTAR_DECLSPEC bool MORTAR_PutAudioStreamData(MORTAR_AudioStream* stream, const void* buf, int len);

#include "mortar_end.h"
