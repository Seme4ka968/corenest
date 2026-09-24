#include "core_loader.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
  #include <windows.h>
  #define LIB_OPEN(p)   ((void*)LoadLibraryA(p))
  #define LIB_SYM(h,n)  ((void*)GetProcAddress((HMODULE)(h), (n)))
  #define LIB_CLOSE(h)  FreeLibrary((HMODULE)(h))
#else
  #include <dlfcn.h>
  #define LIB_OPEN(p)   dlopen((p), RTLD_LAZY | RTLD_LOCAL)
  #define LIB_SYM(h,n)  dlsym((h), (n))
  #define LIB_CLOSE(h)  dlclose((h))
#endif

#define LOAD_SYM(api, name)                                       \
    do {                                                          \
        *(void**)(&(api)->name) = LIB_SYM((api)->handle, #name);  \
        if (!(api)->name) {                                       \
            fprintf(stderr, "core: missing symbol %s\n", #name);  \
            core_unload(api);                                     \
            return false;                                         \
        }                                                         \
    } while (0)

bool core_load(const char *path, core_api_t *api) {
    memset(api, 0, sizeof(*api));

    api->handle = LIB_OPEN(path);
    if (!api->handle) {
        fprintf(stderr, "core: cannot load '%s'\n", path);
        return false;
    }

    LOAD_SYM(api, retro_init);
    LOAD_SYM(api, retro_deinit);
    LOAD_SYM(api, retro_api_version);
    LOAD_SYM(api, retro_get_system_info);
    LOAD_SYM(api, retro_get_system_av_info);
    LOAD_SYM(api, retro_set_environment);
    LOAD_SYM(api, retro_set_video_refresh);
    LOAD_SYM(api, retro_set_audio_sample);
    LOAD_SYM(api, retro_set_audio_sample_batch);
    LOAD_SYM(api, retro_set_input_poll);
    LOAD_SYM(api, retro_set_input_state);
    LOAD_SYM(api, retro_load_game);
    LOAD_SYM(api, retro_unload_game);
    LOAD_SYM(api, retro_run);
    LOAD_SYM(api, retro_reset);

    *(void**)(&api->retro_set_controller_port_device) = LIB_SYM(api->handle, "retro_set_controller_port_device");
    *(void**)(&api->retro_serialize_size)             = LIB_SYM(api->handle, "retro_serialize_size");
    *(void**)(&api->retro_serialize)                  = LIB_SYM(api->handle, "retro_serialize");
    *(void**)(&api->retro_unserialize)                = LIB_SYM(api->handle, "retro_unserialize");
    *(void**)(&api->retro_get_memory_data)            = LIB_SYM(api->handle, "retro_get_memory_data");
    *(void**)(&api->retro_get_memory_size)            = LIB_SYM(api->handle, "retro_get_memory_size");

    return true;
}

void core_unload(core_api_t *api) {
    if (api->handle) {
        LIB_CLOSE(api->handle);
        api->handle = NULL;
    }
}
