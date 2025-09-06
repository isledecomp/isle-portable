#pragma once

#include "SDL.h"

#include <map>
#include <mutex>

// https://wiki.libsdl.org/SDL3/README-migration#sdl_audioh

#define SDL_DestroyAudioStream SDL_FreeAudioStream

#define SDL_AUDIO_F32 AUDIO_F32SYS

#define SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK NULL

typedef void SDL_AudioStreamCallback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount);

struct ShimAudioCtx {
	SDL_AudioStream* stream;
	SDL_AudioStreamCallback* callback;
	void *userdata;
	SDL_AudioSpec obtained;
};

static std::map<SDL_AudioDeviceID, ShimAudioCtx> g_audioCtxs;
static SDL_mutex *g_audioMutex = NULL;

static void SDLCALL shim_audio_callback(void *userdata, Uint8 *stream, int len) {
	SDL_AudioDeviceID devid = reinterpret_cast<uintptr_t>(userdata);

	SDL_LockMutex(g_audioMutex);
	const auto it = g_audioCtxs.find(devid);
	if (it == g_audioCtxs.end()) {
		SDL_UnlockMutex(g_audioMutex);
		SDL_memset(stream, 0, len);
		return;
	}

	ShimAudioCtx &ctx = it->second;
	SDL_UnlockMutex(g_audioMutex);

	// How many sample frames the device is asking for
	const int frame_size = (SDL_AUDIO_BITSIZE(ctx.obtained.format) / 8) * ctx.obtained.channels;
	const int needed_frames = len / frame_size;

	int total = (SDL_AudioStreamAvailable(ctx.stream) / frame_size) + needed_frames;
	// SDL3 callback pushes more into the stream
	ctx.callback(ctx.userdata, ctx.stream, needed_frames, total);

	int got = SDL_AudioStreamGet(ctx.stream, stream, len);
	if (got < len) {
		SDL_memset(stream + got, 0, len - got);
	}
}

#define SDL_ResumeAudioDevice(device) SDL_PauseAudioDevice(device, 0)
#define SDL_PutAudioStreamData(...) (SDL_AudioStreamPut(__VA_ARGS__) == 0)

inline SDL_AudioDeviceID SDL_GetAudioStreamDevice(SDL_AudioStream* stream)
{
	SDL_LockMutex(g_audioMutex);
	const auto it = g_audioCtxs.find(reinterpret_cast<uintptr_t>(stream));
	if (it == g_audioCtxs.end()) {
		return 0;
	}
	return it->first;
}

inline SDL_AudioStream * SDL_OpenAudioDeviceStream(const char* devid, const SDL_AudioSpec* desired, SDL_AudioStreamCallback callback, void* userdata)
{
	SDL_AudioSpec obtained{};
	SDL_AudioSpec desired2 = *desired;
	desired2.callback = shim_audio_callback;
	desired2.userdata = reinterpret_cast<void*>(static_cast<uintptr_t>(0));
	SDL_AudioDeviceID device = SDL_OpenAudioDevice(devid, 0, &desired2, &obtained, 0);
	SDL_AudioStream* stream = SDL_NewAudioStream(
			desired->format, desired->channels, desired->freq,
			obtained.format, obtained.channels, obtained.freq
			);
	if (!stream) {
		SDL_CloseAudioDevice(device);
		return nullptr;
	}

	{
		SDL_LockMutex(g_audioMutex);
		ShimAudioCtx ctx;
		ctx.stream   = stream;
		ctx.callback = callback;
		ctx.userdata = userdata;
		ctx.obtained = obtained;
		g_audioCtxs[device] = ctx;
		SDL_UnlockMutex(g_audioMutex);
	}

	SDL_LockAudioDevice(device);
	desired2.userdata = reinterpret_cast<void*>(static_cast<uintptr_t>(device));
	SDL_UnlockAudioDevice(device);

	return stream;
}
