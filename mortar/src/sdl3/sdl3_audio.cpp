#include "mortar/mortar_audio.h"
#include "sdl3_internal.h"

#include <stdlib.h>

static SDL_AudioFormat audioformat_mortar_to_sdl3(MORTAR_AudioFormat format)
{
	switch (format) {
	case MORTAR_AUDIO_F32:
		return SDL_AUDIO_F32;
	default:
		abort();
	}
}

MORTAR_AudioStream* MORTAR_EX_OpenAudioPlaybackDevice(
	const MORTAR_AudioSpec* spec,
	MORTAR_AudioStreamCallback callback,
	void* userdata
)
{
	SDL_AudioSpec sdl3_spec;
	sdl3_spec.format = audioformat_mortar_to_sdl3(spec->format);
	sdl3_spec.channels = spec->channels;
	sdl3_spec.freq = spec->freq;
	return (MORTAR_AudioStream*) SDL_OpenAudioDeviceStream(
		SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
		&sdl3_spec,
		(SDL_AudioStreamCallback) callback,
		userdata
	);
}

void MORTAR_DestroyAudioStream(MORTAR_AudioStream* stream)
{
	SDL_DestroyAudioStream((SDL_AudioStream*) stream);
}

bool MORTAR_EX_ResumeAudioDevice(MORTAR_AudioStream* stream)
{
	return SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice((SDL_AudioStream*) stream));
}

bool MORTAR_PutAudioStreamData(MORTAR_AudioStream* stream, const void* buf, int len)
{
	return SDL_PutAudioStreamData((SDL_AudioStream*) stream, buf, len);
}
