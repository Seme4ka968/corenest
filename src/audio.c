#include "audio.h"
#include <SDL2/SDL.h>
#include <stdio.h>

static SDL_AudioDeviceID g_dev = 0;

bool audio_init(double sample_rate) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "audio: SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq     = (int)sample_rate;
    want.format   = AUDIO_S16SYS;
    want.channels = 2;
    want.samples  = 2048;

    g_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!g_dev) {
        fprintf(stderr, "audio: OpenAudioDevice failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_PauseAudioDevice(g_dev, 0);
    return true;
}

void audio_deinit(void) {
    if (g_dev) {
        SDL_CloseAudioDevice(g_dev);
        g_dev = 0;
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void audio_push_sample(int16_t left, int16_t right) {
    if (!g_dev) return;
    int16_t buf[2] = { left, right };
    SDL_QueueAudio(g_dev, buf, sizeof(buf));
}

size_t audio_push_batch(const int16_t *data, size_t frames) {
    if (!g_dev || !data) return 0;
    SDL_QueueAudio(g_dev, data, (Uint32)(frames * 2 * sizeof(int16_t)));
    return frames;
}
