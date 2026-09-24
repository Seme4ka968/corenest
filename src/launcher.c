#include "launcher.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "libretro.h"
#include "core_loader.h"
#include "environment.h"
#include "video.h"
#include "audio.h"
#include "input.h"

#ifdef _WIN32
  #include <direct.h>
  #define CHDIR _chdir
#else
  #include <unistd.h>
  #define CHDIR chdir
#endif

static core_api_t g_core;
static int g_running = 1;
static enum retro_pixel_format g_fmt = RETRO_PIXEL_FORMAT_0RGB1555;

static char g_root[1024] = ".";

static void cb_video(const void *data, unsigned w, unsigned h, size_t pitch) {
    if (!data) { video_present(); return; }
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

static int ends_with(const char *s, const char *suffix) {
    if (!s || !suffix) return 0;
    size_t ls = strlen(s), lx = strlen(suffix);
    if (lx > ls) return 0;
    return strcmp(s + ls - lx, suffix) == 0;
}

static char *dup_str(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char*)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static int dir_exists(const char *path) {
    DIR *d = opendir(path);
    if (d) { closedir(d); return 1; }
    return 0;
}

static int file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

static char *find_first(const char *dir, const char *ext) {
    DIR *d = opendir(dir);
    if (!d) return NULL;

    char *result = NULL;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        if (!ends_with(e->d_name, ext)) continue;

        size_t len = strlen(dir) + 1 + strlen(e->d_name) + 1;
        result = (char*)malloc(len);
        if (result) {
            snprintf(result, len, "%s/%s", dir, e->d_name);
        }
        break;
    }
    closedir(d);
    return result;
}

static char *find_in_roots(const char *subdir, const char *ext) {
    char path[1200];

    snprintf(path, sizeof(path), "%s/%s", g_root, subdir);
    char *r = find_first(path, ext);
    if (r) return r;

    snprintf(path, sizeof(path), "%s", subdir);
    r = find_first(path, ext);
    if (r) return r;

    snprintf(path, sizeof(path), "../%s", subdir);
    r = find_first(path, ext);
    if (r) return r;

    return NULL;
}

static int file_exists_in_roots(const char *subdir, const char *name) {
    char path[1200];

    snprintf(path, sizeof(path), "%s/%s/%s", g_root, subdir, name);
    if (file_exists(path)) return 1;

    snprintf(path, sizeof(path), "%s/%s", subdir, name);
    if (file_exists(path)) return 1;

    snprintf(path, sizeof(path), "../%s/%s", subdir, name);
    if (file_exists(path)) return 1;

    return 0;
}

static void pause_if_click(int argc) {
    if (argc <= 1) {
        printf("\nPress Enter to exit...\n");
        fflush(stdout);
        getchar();
    }
}

static const char *pixel_format_name(enum retro_pixel_format fmt) {
    switch (fmt) {
    case RETRO_PIXEL_FORMAT_0RGB1555: return "0RGB1555";
    case RETRO_PIXEL_FORMAT_XRGB8888: return "XRGB8888";
    case RETRO_PIXEL_FORMAT_RGB565:   return "RGB565";
    default:                          return "UNKNOWN";
    }
}

/* Находим корень проекта:
   1. Рядом с .exe (SDL_GetBasePath)
   2. Текущая директория
   3. Родительская (../)
*/
static void find_root(void) {
    char *base = SDL_GetBasePath();
    if (base) {
        size_t len = strlen(base);
        if (len > 1 && (base[len-1] == '/' || base[len-1] == '\\'))
            base[len-1] = 0;

        char test[1200];
        snprintf(test, sizeof(test), "%s/../cores", base);
        if (dir_exists(test)) {
            snprintf(g_root, sizeof(g_root), "%s/..", base);
            SDL_free(base);
            return;
        }

        snprintf(test, sizeof(test), "%s/cores", base);
        if (dir_exists(test)) {
            snprintf(g_root, sizeof(g_root), "%s", base);
            SDL_free(base);
            return;
        }
        SDL_free(base);
    }

    if (dir_exists("cores")) {
        snprintf(g_root, sizeof(g_root), ".");
        return;
    }

    if (dir_exists("../cores")) {
        snprintf(g_root, sizeof(g_root), "..");
        return;
    }

    snprintf(g_root, sizeof(g_root), ".");
}

int launcher_run(int argc, char **argv) {
    printf("[launcher] CoreNest v0.1.0\n");
    fflush(stdout);

    if (SDL_Init(0) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        pause_if_click(argc);
        return 1;
    }

    find_root();
    printf("[launcher] root: %s\n", g_root);
    printf("[launcher] argc=%d\n", argc);
    fflush(stdout);

    char *core_path = NULL;
    char *rom_path  = NULL;
    int   free_core = 0;
    int   free_rom  = 0;

    if (argc >= 3) {
        core_path = dup_str(argv[1]);
        rom_path  = dup_str(argv[2]);
        free_core = 1;
        free_rom  = 1;
    } else if (argc == 2) {
        rom_path  = dup_str(argv[1]);
        free_rom  = 1;

        core_path = find_in_roots("cores", ".dll");
        if (!core_path) core_path = find_in_roots("cores", ".so");
        free_core = 1;
    } else {
        static const struct {
            const char *ext;
            const char *core;
        } table[] = {
            { ".gb",  "gambatte"          },
            { ".gbc", "gambatte"          },
            { ".gba", "mgba"              },
            { ".nes", "nestopia"          },
            { ".sfc", "snes9x"            },
            { ".smc", "snes9x"            },
            { ".md",  "genesis_plus_gx"   },
            { ".gen", "genesis_plus_gx"   },
            { ".sms", "genesis_plus_gx"   },
            { ".gg",  "genesis_plus_gx"   },
            { ".pce", "mednafen_pce_fast" },
        };
        const size_t table_n = sizeof(table) / sizeof(table[0]);

        for (size_t i = 0; i < table_n && !rom_path; i++) {
            rom_path = find_in_roots("roms", table[i].ext);
            if (!rom_path) continue;

            char name[256];

            snprintf(name, sizeof(name), "%s_libretro.dll", table[i].core);
            if (file_exists_in_roots("cores", name)) {
                char p[1200];
                snprintf(p, sizeof(p), "%s/cores/%s", g_root, name);
                if (file_exists(p)) {
                    core_path = dup_str(p);
                    break;
                }
                snprintf(p, sizeof(p), "cores/%s", name);
                if (file_exists(p)) {
                    core_path = dup_str(p);
                    break;
                }
                snprintf(p, sizeof(p), "../cores/%s", name);
                if (file_exists(p)) {
                    core_path = dup_str(p);
                    break;
                }
            }

            snprintf(name, sizeof(name), "%s_libretro.so", table[i].core);
            if (file_exists_in_roots("cores", name)) {
                char p[1200];
                snprintf(p, sizeof(p), "%s/cores/%s", g_root, name);
                if (file_exists(p)) {
                    core_path = dup_str(p);
                    break;
                }
                snprintf(p, sizeof(p), "cores/%s", name);
                if (file_exists(p)) {
                    core_path = dup_str(p);
                    break;
                }
                snprintf(p, sizeof(p), "../cores/%s", name);
                if (file_exists(p)) {
                    core_path = dup_str(p);
                    break;
                }
            }

            fprintf(stderr, "[launcher] warn: no core for %s\n", table[i].ext);
        }

        if (!core_path) {
            core_path = find_in_roots("cores", ".dll");
            if (!core_path) core_path = find_in_roots("cores", ".so");
        }
        if (!rom_path) {
            rom_path = find_in_roots("roms", ".gb");
            if (!rom_path) rom_path = find_in_roots("roms", ".gba");
            if (!rom_path) rom_path = find_in_roots("roms", ".nes");
            if (!rom_path) rom_path = find_in_roots("roms", ".sfc");
        }

        free_core = 1;
        free_rom  = 1;
    }

    if (!core_path) {
        fprintf(stderr, "error: no core found\n");
        pause_if_click(argc);
        if (free_core) free(core_path);
        if (free_rom)  free(rom_path);
        return 1;
    }
    if (!rom_path) {
        fprintf(stderr, "error: no rom found\n");
        pause_if_click(argc);
        if (free_core) free(core_path);
        if (free_rom)  free(rom_path);
        return 1;
    }

    printf("[launcher] core: %s\n", core_path);
    printf("[launcher] rom:  %s\n", rom_path);
    fflush(stdout);

    if (!core_load(core_path, &g_core)) {
        pause_if_click(argc);
        if (free_core) free(core_path);
        if (free_rom)  free(rom_path);
        return 1;
    }
    printf("[launcher] core_load ok\n"); fflush(stdout);

    env_set_paths(g_root, g_root);

    g_core.retro_set_environment(cb_environment);
    g_core.retro_set_video_refresh(cb_video);
    g_core.retro_set_audio_sample(cb_audio);
    g_core.retro_set_audio_sample_batch(cb_audio_batch);
    g_core.retro_set_input_poll(cb_input_poll);
    g_core.retro_set_input_state(cb_input_state);

    printf("[launcher] retro_init\n"); fflush(stdout);
    g_core.retro_init();

    struct retro_system_info sys_info;
    memset(&sys_info, 0, sizeof(sys_info));
    g_core.retro_get_system_info(&sys_info);
    printf("[launcher] core: %s %s\n",
           sys_info.library_name ? sys_info.library_name : "?",
           sys_info.library_version ? sys_info.library_version : "?");
    fflush(stdout);

    FILE *f = fopen(rom_path, "rb");
    if (!f) {
        fprintf(stderr, "cannot open rom: %s\n", rom_path);
        pause_if_click(argc);
        if (free_core) free(core_path);
        if (free_rom)  free(rom_path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long rom_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *rom_data = malloc((size_t)rom_size);
    if (!rom_data) { fclose(f); fprintf(stderr, "oom\n"); return 1; }
    fread(rom_data, 1, (size_t)rom_size, f);
    fclose(f);
    printf("[launcher] rom loaded (%ld bytes)\n", rom_size); fflush(stdout);

    struct retro_game_info game = {0};
    game.path = rom_path;
    game.data = rom_data;
    game.size = (size_t)rom_size;

    printf("[launcher] retro_load_game\n"); fflush(stdout);
    if (!g_core.retro_load_game(&game)) {
        fprintf(stderr, "core: retro_load_game failed\n");
        pause_if_click(argc);
        free(rom_data);
        if (free_core) free(core_path);
        if (free_rom)  free(rom_path);
        return 1;
    }
    printf("[launcher] retro_load_game ok\n"); fflush(stdout);

    struct retro_system_av_info av;
    memset(&av, 0, sizeof(av));
    g_core.retro_get_system_av_info(&av);

    printf("[launcher] av: %ux%u @ %.2f fps, %.0f Hz\n",
           av.geometry.base_width, av.geometry.base_height,
           av.timing.fps, av.timing.sample_rate);
    fflush(stdout);

    printf("[launcher] video_init\n"); fflush(stdout);
    if (!video_init(av.geometry.base_width, av.geometry.base_height)) {
        pause_if_click(argc);
        free(rom_data);
        if (free_core) free(core_path);
        if (free_rom)  free(rom_path);
        return 1;
    }

    printf("[launcher] audio_init\n"); fflush(stdout);
    if (!audio_init(av.timing.sample_rate)) {
        pause_if_click(argc);
        free(rom_data);
        if (free_core) free(core_path);
        if (free_rom)  free(rom_path);
        return 1;
    }

    printf("[launcher] input_init\n"); fflush(stdout);
    if (!input_init()) {
        pause_if_click(argc);
        free(rom_data);
        if (free_core) free(core_path);
        if (free_rom)  free(rom_path);
        return 1;
    }

    if (g_core.retro_set_controller_port_device)
        g_core.retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);

    printf("[launcher] pixel format: %s\n", pixel_format_name(g_fmt));
    printf("[launcher] entering main loop\n"); fflush(stdout);

    while (g_running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) g_running = 0;
        }
        g_core.retro_run();
    }

    printf("[launcher] exit loop\n"); fflush(stdout);

    g_core.retro_unload_game();
    g_core.retro_deinit();
    core_unload(&g_core);

    input_deinit();
    audio_deinit();
    video_deinit();
    SDL_Quit();

    free(rom_data);
    if (free_core) free(core_path);
    if (free_rom)  free(rom_path);

    printf("[launcher] done\n"); fflush(stdout);
    return 0;
}
