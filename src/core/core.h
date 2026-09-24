#ifndef CORENEST_CORE_H
#define CORENEST_CORE_H

#include <stdbool.h>
#include "libretro.h"

typedef struct {
    void *handle;

    retro_init_t retro_init;
    retro_deinit_t retro_deinit;
    retro_api_version_t retro_api_version;
    retro_get_system_info_t retro_get_system_info;
    retro_get_system_av_info_t retro_get_system_av_info;

    retro_set_environment_t retro_set_environment;
    retro_set_video_refresh_t retro_set_video_refresh;
    retro_set_audio_sample_t retro_set_audio_sample;
    retro_set_audio_sample_batch_t retro_set_audio_sample_batch;
    retro_set_input_poll_t retro_set_input_poll;
    retro_set_input_state_t retro_set_input_state;

    retro_load_game_t retro_load_game;
    retro_unload_game_t retro_unload_game;
    retro_run_t retro_run;
    retro_reset_t retro_reset;

    retro_serialize_size_t retro_serialize_size;
    retro_serialize_t retro_serialize;
    retro_unserialize_t retro_unserialize;
    retro_get_memory_data_t retro_get_memory_data;
    retro_get_memory_size_t retro_get_memory_size;
    retro_set_controller_port_device_t retro_set_controller_port_device;
} core_api_t;

typedef struct {
    char path[1024];
    char name[128];
    char version[128];
    struct retro_system_info system;
    struct retro_system_av_info av;
    bool loaded;
    bool game_loaded;
} core_runtime_t;

bool core_load(const char *path, core_api_t *out);
void core_unload(core_api_t *core);
bool core_start(core_runtime_t *runtime, core_api_t *api, const char *rom_path);
void core_stop(core_runtime_t *runtime, core_api_t *api);
void core_run(core_runtime_t *runtime, core_api_t *api);
void core_reset(core_runtime_t *runtime, core_api_t *api);

#endif
