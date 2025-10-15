#pragma once

#include "SDL.h"

#include <map>
#include <mutex>

// https://wiki.libsdl.org/SDL3/README-migration#sdl_audioh

#define SDL_AUDIO_F32 AUDIO_F32SYS

#define SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK NULL

typedef void SDL_AudioStreamCallback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount);

struct ShimAudioCtx {
	SDL_AudioDeviceID device;
	SDL_AudioStream* stream;
	SDL_AudioStreamCallback* callback;
	void *userdata;
	SDL_AudioSpec obtained;
};

static std::map<SDL_AudioStream*, ShimAudioCtx*> g_audioCtxs;
static SDL_mutex *g_audioMutex = SDL_CreateMutex();

static void shim_audio_callback(void *userdata, Uint8 *data_stream, int len) {
	const auto ctx = static_cast<ShimAudioCtx*>(userdata);
	SDL_AudioStream* audio_stream = ctx->stream;

	int available = SDL_AudioStreamAvailable(audio_stream);

	int additional_amount = len - available;
	if (additional_amount < 0)
		additional_amount = 0;

	ctx->callback(ctx->userdata, audio_stream, additional_amount, len);

	int retrieved = SDL_AudioStreamGet(audio_stream, data_stream, len);
	if (retrieved < len) {
		memset(data_stream + retrieved, ctx->obtained.silence, len - retrieved);
	}
}

#define SDL_ResumeAudioDevice(device) SDL_PauseAudioDevice(device, 0)
#define SDL_PutAudioStreamData(...) (SDL_AudioStreamPut(__VA_ARGS__) == 0)

inline SDL_AudioDeviceID SDL_GetAudioStreamDevice(SDL_AudioStream* stream)
{
	SDL_LockMutex(g_audioMutex);
	const auto it = g_audioCtxs.find(stream);
	if (it == g_audioCtxs.end()) {
		return 0;
	}
	SDL_UnlockMutex(g_audioMutex);
	return it->second->device;
}

inline SDL_AudioStream * SDL_OpenAudioDeviceStream(const char* devid, const SDL_AudioSpec* desired, SDL_AudioStreamCallback callback, void* userdata)
{
	SDL_AudioSpec obtained{};
	SDL_AudioSpec desired2 = *desired;

	auto ctx = new ShimAudioCtx();
	ctx->callback = callback;
	ctx->userdata = userdata;
	ctx->stream = nullptr;

	desired2.callback = shim_audio_callback;
	desired2.userdata = ctx;
	const SDL_AudioDeviceID device = SDL_OpenAudioDevice(devid, 0, &desired2, &obtained, 0);
	if (device <= 0) {
		delete ctx;
		return NULL;
	}

	SDL_AudioStream* stream = SDL_NewAudioStream(
			desired->format, desired->channels, desired->freq,
			obtained.format, obtained.channels, obtained.freq
			);
	if (!stream) {
		SDL_CloseAudioDevice(device);
		delete ctx;
		return NULL;
	}

	ctx->device   = device;
	ctx->stream   = stream;
	ctx->obtained = obtained;

	SDL_LockMutex(g_audioMutex);
	g_audioCtxs[stream] = ctx;
	SDL_UnlockMutex(g_audioMutex);

	return stream;
}

inline SDL_bool SDL_DestroyAudioStream(SDL_AudioStream* stream)
{
	SDL_LockMutex(g_audioMutex);
	if (const auto it = g_audioCtxs.find(stream); it != g_audioCtxs.end()) {
		SDL_CloseAudioDevice(it->second->device);
		delete it->second;
		g_audioCtxs.erase(it);
		SDL_UnlockMutex(g_audioMutex);
	}
	SDL_UnlockMutex(g_audioMutex);

	SDL_FreeAudioStream(stream);
	return true;
}
