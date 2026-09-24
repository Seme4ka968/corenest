#include "core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void *open_library(const char *path) {
#ifdef _WIN32
    return (void *)LoadLibraryA(path);
#else
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

static void close_library(void *handle) {
#ifdef _WIN32
    if (handle) FreeLibrary((HMODULE)handle);
#else
    if (handle) dlclose(handle);
#endif
}

static void *load_symbol(void *handle, const char *name) {
#ifdef _WIN32
    return (void *)GetProcAddress((HMODULE)handle, name);
#else
    return dlsym(handle, name);
#endif
}

#define LOAD_REQUIRED(field, symbol) do { \
    out->field = (void *)load_symbol(out->handle, symbol); \
    if (!out->field) goto fail; \
} while (0)

bool core_load(const char *path, core_api_t *out) {
    if (!path || !out) return false;

    memset(out, 0, sizeof(*out));
    out->handle = open_library(path);
    if (!out->handle) return false;

    LOAD_REQUIRED(retro_set_environment, "retro_set_environment");
    LOAD_REQUIRED(retro_set_video_refresh, "retro_set_video_refresh");
    LOAD_REQUIRED(retro_set_audio_sample, "retro_set_audio_sample");
    LOAD_REQUIRED(retro_set_audio_sample_batch, "retro_set_audio_sample_batch");
    LOAD_REQUIRED(retro_set_input_poll, "retro_set_input_poll");
    LOAD_REQUIRED(retro_set_input_state, "retro_set_input_state");
    LOAD_REQUIRED(retro_init, "retro_init");
    LOAD_REQUIRED(retro_deinit, "retro_deinit");
    LOAD_REQUIRED(retro_get_system_info, "retro_get_system_info");
    LOAD_REQUIRED(retro_get_system_av_info, "retro_get_system_av_info");
    LOAD_REQUIRED(retro_load_game, "retro_load_game");
    LOAD_REQUIRED(retro_unload_game, "retro_unload_game");
    LOAD_REQUIRED(retro_run, "retro_run");
    LOAD_REQUIRED(retro_reset, "retro_reset");

    out->retro_api_version = (retro_api_version_t)load_symbol(out->handle, "retro_api_version");
    out->retro_serialize_size = (retro_serialize_size_t)load_symbol(out->handle, "retro_serialize_size");
    out->retro_serialize = (retro_serialize_t)load_symbol(out->handle, "retro_serialize");
    out->retro_unserialize = (retro_unserialize_t)load_symbol(out->handle, "retro_unserialize");
    out->retro_get_memory_data = (retro_get_memory_data_t)load_symbol(out->handle, "retro_get_memory_data");
    out->retro_get_memory_size = (retro_get_memory_size_t)load_symbol(out->handle, "retro_get_memory_size");
    out->retro_set_controller_port_device =
        (retro_set_controller_port_device_t)load_symbol(out->handle, "retro_set_controller_port_device");

    return true;

fail:
    close_library(out->handle);
    memset(out, 0, sizeof(*out));
    return false;
}

void core_unload(core_api_t *core) {
    if (!core) return;
    close_library(core->handle);
    memset(core, 0, sizeof(*core));
}

static bool load_content(core_runtime_t *runtime, core_api_t *api, const char *rom_path) {
    FILE *file = fopen(rom_path, "rb");
    if (!file) return false;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }

    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        return false;
    }

    rewind(file);

    void *data = malloc((size_t)size);
    if (!data) {
        fclose(file);
        return false;
    }

    const size_t expected = (size_t)size;
    const size_t actual = fread(data, 1, expected, file);
    fclose(file);

    if (actual != expected) {
        free(data);
        return false;
    }

    struct retro_game_info game = {
        .path = rom_path,
        .data = data,
        .size = expected,
        .meta = NULL
    };

    const bool loaded = api->retro_load_game(&game);
    free(data);

    if (!loaded) return false;

    runtime->game_loaded = true;
    memset(&runtime->av, 0, sizeof(runtime->av));
    api->retro_get_system_av_info(&runtime->av);
    return true;
}

bool core_start(core_runtime_t *runtime, core_api_t *api, const char *rom_path) {
    if (!runtime || !api || !rom_path || !api->retro_init) return false;

    memset(runtime, 0, sizeof(*runtime));
    api->retro_init();
    api->retro_get_system_info(&runtime->system);

    if (!load_content(runtime, api, rom_path)) {
        api->retro_deinit();
        return false;
    }

    snprintf(runtime->path, sizeof(runtime->path), "%s", rom_path);
    snprintf(runtime->name, sizeof(runtime->name), "%s",
             runtime->system.library_name ? runtime->system.library_name : "Unknown Core");
    snprintf(runtime->version, sizeof(runtime->version), "%s",
             runtime->system.library_version ? runtime->system.library_version : "Unknown");

    runtime->loaded = true;
    return true;
}

void core_stop(core_runtime_t *runtime, core_api_t *api) {
    if (!runtime || !api) return;

    if (runtime->game_loaded && api->retro_unload_game)
        api->retro_unload_game();

    if (runtime->loaded && api->retro_deinit)
        api->retro_deinit();

    memset(runtime, 0, sizeof(*runtime));
}

void core_run(core_runtime_t *runtime, core_api_t *api) {
    if (runtime && api && runtime->game_loaded && api->retro_run)
        api->retro_run();
}

void core_reset(core_runtime_t *runtime, core_api_t *api) {
    if (runtime && api && runtime->game_loaded && api->retro_reset)
        api->retro_reset();
}
