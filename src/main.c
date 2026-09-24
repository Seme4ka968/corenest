#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libretro.h"
#include "core_loader.h"
#include "environment.h"
#include "video.h"
#include "audio.h"
#include "input.h"

static core_api_t g_core;
static int g_running = 1;
static enum retro_pixel_format g_fmt = RETRO_PIXEL_FORMAT_0RGB1555;

static void cb_video(const void *data, unsigned w, unsigned h, size_t pitch) {
    if (!data) {
        video_present();
        return;
    }
    video_refresh(data, w, h, pitch, g_fmt);
    video_present();
}

static void cb_audio(int16_t left, int16_t right) {
    audio_push_sample(left, right);
}

static size_t cb_audio_batch(const int16_t *data, size_t frames) {
    return audio_push_batch(data, frames);
}

static void cb_input_poll(void) {
    input_poll();
}

static int16_t cb_input_state(unsigned port, unsigned device,
                              unsigned index, unsigned id)
{
    return input_state(port, device, index, id);
}

static bool cb_environment(unsigned cmd, void *data) {
    if (cmd == RETRO_ENVIRONMENT_SET_PIXEL_FORMAT) {
        g_fmt = *(const enum retro_pixel_format*)data;
        return true;
    }
    return env_callback(cmd, data);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <core.dll> <rom>\n", argv[0]);
        return 1;
    }

    const char *core_path = argv[1];
    const char *rom_path  = argv[2];

    if (SDL_Init(0) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    if (!core_load(core_path, &g_core)) return 1;

    env_set_paths(".", ".");

    g_core.retro_set_environment(cb_environment);
    g_core.retro_set_video_refresh(cb_video);
    g_core.retro_set_audio_sample(cb_audio);
    g_core.retro_set_audio_sample_batch(cb_audio_batch);
    g_core.retro_set_input_poll(cb_input_poll);
    g_core.retro_set_input_state(cb_input_state);

    g_core.retro_init();

    FILE *f = fopen(rom_path, "rb");
    if (!f) { fprintf(stderr, "cannot open rom: %s\n", rom_path); return 1; }
    fseek(f, 0, SEEK_END);
    long rom_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *rom_data = malloc(rom_size);
    fread(rom_data, 1, rom_size, f);
    fclose(f);

    struct retro_game_info game = {0};
    game.path = rom_path;
    game.data = rom_data;
    game.size = (size_t)rom_size;

    if (!g_core.retro_load_game(&game)) {
        fprintf(stderr, "core: retro_load_game failed\n");
        return 1;
    }

    struct retro_system_av_info av;
    memset(&av, 0, sizeof(av));
    g_core.retro_get_system_av_info(&av);

    if (!video_init(av.geometry.base_width, av.geometry.base_height)) return 1;
    if (!audio_init(av.timing.sample_rate)) return 1;
    if (!input_init()) return 1;

    if (g_core.retro_set_controller_port_device)
        g_core.retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);

    while (g_running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) g_running = 0;
        }
        g_core.retro_run();
    }

    g_core.retro_unload_game();
    g_core.retro_deinit();
    core_unload(&g_core);

    input_deinit();
    audio_deinit();
    video_deinit();
    SDL_Quit();

    free(rom_data);
    return 0;
}
