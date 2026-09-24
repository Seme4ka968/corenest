#include "launcher.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>

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

#define MAX_FILES 128

static core_api_t g_core;
static int g_running = 1;
static enum retro_pixel_format g_fmt = RETRO_PIXEL_FORMAT_0RGB1555;
static char g_root[1024] = ".";

/* --- Колбэки libretro --- */
static void cb_video(const void *data, unsigned w, unsigned h, size_t pitch) {
    if (!data) { video_present(); return; }
    video_refresh(data, w, h, pitch, g_fmt);
    video_present();
}
static void cb_audio(int16_t left, int16_t right) { audio_push_sample(left, right); }
static size_t cb_audio_batch(const int16_t *data, size_t frames) { return audio_push_batch(data, frames); }
static void cb_input_poll(void) { input_poll(); }
static int16_t cb_input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
    return input_state(port, device, index, id);
}
static bool cb_environment(unsigned cmd, void *data) {
    if (cmd == RETRO_ENVIRONMENT_SET_PIXEL_FORMAT) {
        g_fmt = *(const enum retro_pixel_format*)data;
        return true;
    }
    return env_callback(cmd, data);
}

/* --- Утилиты --- */
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

static char *find_first_or_null(const char *dir, const char *ext) {
    DIR *d = opendir(dir);
    if (!d) return NULL;

    char *result = NULL;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        if (!ends_with(e->d_name, ext)) continue;
        size_t len = strlen(dir) + 1 + strlen(e->d_name) + 1;
        result = (char*)malloc(len);
        if (result) snprintf(result, len, "%s/%s", dir, e->d_name);
        break;
    }
    closedir(d);
    return result;
}

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

static char **list_files(const char *dir, const char *ext, int *count) {
    *count = 0;
    DIR *d = opendir(dir);
    if (!d) return NULL;

    char **out = (char**)malloc(sizeof(char*) * MAX_FILES);
    if (!out) { closedir(d); return NULL; }

    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        if (!ends_with(e->d_name, ext)) continue;
        if (*count >= MAX_FILES) break;

        size_t len = strlen(dir) + 1 + strlen(e->d_name) + 1;
        out[*count] = (char*)malloc(len);
        if (out[*count]) {
            snprintf(out[*count], len, "%s/%s", dir, e->d_name);
            (*count)++;
        }
    }
    closedir(d);

    if (*count > 0) qsort(out, *count, sizeof(char*), cmp_str);
    return out;
}

static void free_list(char **list, int count) {
    if (!list) return;
    for (int i = 0; i < count; i++) free(list[i]);
    free(list);
}

static const char *basename_of(const char *path) {
    const char *p = strrchr(path, '/');
    if (!p) p = strrchr(path, '\\');
    return p ? p + 1 : path;
}

static char *pick_core_for_rom(const char *rom_path) {
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
    const size_t n = sizeof(table) / sizeof(table[0]);

    for (size_t i = 0; i < n; i++) {
        if (!ends_with(rom_path, table[i].ext)) continue;

        char path[1200];

        snprintf(path, sizeof(path), "%s/cores/%s_libretro.dll", g_root, table[i].core);
        if (file_exists(path)) return dup_str(path);

        snprintf(path, sizeof(path), "%s/cores/%s_libretro.so", g_root, table[i].core);
        if (file_exists(path)) return dup_str(path);

        snprintf(path, sizeof(path), "cores/%s_libretro.dll", table[i].core);
        if (file_exists(path)) return dup_str(path);

        snprintf(path, sizeof(path), "cores/%s_libretro.so", table[i].core);
        if (file_exists(path)) return dup_str(path);

        return NULL;
    }
    return NULL;
}

static int select_from_list(const char *title, char **list, int count) {
    printf("\n");
    printf("===============================================\n");
    printf("  CoreNest v0.1.0 - %s\n", title);
    printf("===============================================\n\n");

    for (int i = 0; i < count; i++) {
        printf("  [%d] %s\n", i + 1, basename_of(list[i]));
    }
    printf("\n");

    char buf[64];
    while (1) {
        printf("  Select (1-%d), or Q to quit: ", count);
        fflush(stdout);

        if (!fgets(buf, sizeof(buf), stdin)) return -1;

        char *p = buf;
        while (*p && isspace((unsigned char)*p)) p++;
        size_t len = strlen(p);
        while (len > 0 && isspace((unsigned char)p[len-1])) p[--len] = 0;

        if (len == 0) return -1;
        if (p[0] == 'q' || p[0] == 'Q') return -1;

        char *end = NULL;
        long v = strtol(p, &end, 10);
        if (end == p) { printf("  Invalid input, try again.\n"); continue; }
        if (v < 1 || v > count) { printf("  Out of range, try again.\n"); continue; }
        return (int)(v - 1);
    }
}

static void pause_if_click(int argc) {
    if (argc <= 1) {
        printf("\nPress Enter to exit...\n");
        fflush(stdout);
        int c;
        while ((c = getchar()) != '\n' && c != EOF) {}
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

static void find_root(void) {
    char *base = SDL_GetBasePath();
    if (base) {
        size_t len = strlen(base);
        if (len > 1 && (base[len-1] == '/' || base[len-1] == '\\'))
            base[len-1] = 0;

        char test[1200];
        snprintf(test, sizeof(test), "%s/../cores", base);
        if (dir_exists(test)) { snprintf(g_root, sizeof(g_root), "%s/..", base); SDL_free(base); return; }

        snprintf(test, sizeof(test), "%s/cores", base);
        if (dir_exists(test)) { snprintf(g_root, sizeof(g_root), "%s", base); SDL_free(base); return; }
        SDL_free(base);
    }

    if (dir_exists("cores")) { snprintf(g_root, sizeof(g_root), "."); return; }
    if (dir_exists("../cores")) { snprintf(g_root, sizeof(g_root), ".."); return; }
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
    int   cli_scale = 0;       /* 0 = не задан */
    int   cli_pos_x = -1;      /* -1 = не задан */
    int   cli_pos_y = -1;
    int   cli_fullscreen = 0;
    char *cli_core = NULL;
    char *cli_rom  = NULL;

    /* --- Парсинг CLI-флагов --- */
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];

        if (strncmp(a, "--scale=", 8) == 0) {
            cli_scale = atoi(a + 8);
            if (cli_scale < 1) cli_scale = 1;
            if (cli_scale > 8) cli_scale = 8;
        } else if (strncmp(a, "--pos=", 6) == 0) {
            int x = -1, y = -1;
            if (sscanf(a + 6, "%d,%d", &x, &y) == 2) {
                cli_pos_x = x;
                cli_pos_y = y;
            }
        } else if (strcmp(a, "--fullscreen") == 0 || strcmp(a, "-f") == 0) {
            cli_fullscreen = 1;
        } else if (strncmp(a, "--core=", 7) == 0) {
            cli_core = dup_str(a + 7);
        } else if (strncmp(a, "--rom=", 6) == 0) {
            cli_rom = dup_str(a + 6);
        } else if (a[0] != '-') {
            /* Позиционный аргумент */
            if (!cli_core && !core_path && file_exists(a)) {
                if (ends_with(a, ".dll") || ends_with(a, ".so")) {
                    cli_core = dup_str(a);
                } else {
                    cli_rom = dup_str(a);
                }
            } else if (!cli_rom) {
                cli_rom = dup_str(a);
            }
        }
    }

    /* --- Выбор путей --- */
    if (cli_core) {
        core_path = cli_core;
        free_core = 1;
    }
    if (cli_rom) {
        rom_path = cli_rom;
        free_rom = 1;
    }

    if (rom_path && !core_path) {
        core_path = pick_core_for_rom(rom_path);
        if (!core_path) {
            core_path = find_first_or_null("cores", ".dll");
            if (!core_path) core_path = find_first_or_null("cores", ".so");
        }
        free_core = 1;
    }

    if (!core_path || !rom_path) {
        /* --- Меню (как было) --- */
        char path[1200];

        snprintf(path, sizeof(path), "%s/cores", g_root);
        if (!dir_exists(path)) snprintf(path, sizeof(path), "cores");

        int core_count = 0;
        char **cores = list_files(path, ".dll", &core_count);
        if (core_count == 0) {
            free_list(cores, core_count);
            cores = list_files(path, ".so", &core_count);
        }
        if (!cores || core_count == 0) {
            fprintf(stderr, "error: no cores found in %s/\n", path);
            pause_if_click(argc);
            free_list(cores, core_count);
            if (free_core) free(core_path);
            if (free_rom)  free(rom_path);
            return 1;
        }

        snprintf(path, sizeof(path), "%s/roms", g_root);
        if (!dir_exists(path)) snprintf(path, sizeof(path), "roms");

        const char *rom_exts[] = { ".gb", ".gbc", ".gba", ".nes", ".sfc", ".smc",
                                   ".md", ".gen", ".sms", ".gg", ".pce" };
        const size_t rom_exts_n = sizeof(rom_exts) / sizeof(rom_exts[0]);

        char **roms = (char**)malloc(sizeof(char*) * MAX_FILES);
        int rom_count = 0;
        if (roms) {
            for (size_t i = 0; i < rom_exts_n; i++) {
                int c = 0;
                char **tmp = list_files(path, rom_exts[i], &c);
                for (int j = 0; j < c && rom_count < MAX_FILES; j++) {
                    roms[rom_count++] = dup_str(tmp[j]);
                }
                free_list(tmp, c);
            }
            if (rom_count > 0) qsort(roms, rom_count, sizeof(char*), cmp_str);
        }

        if (!roms || rom_count == 0) {
            fprintf(stderr, "error: no roms found in %s/\n", path);
            pause_if_click(argc);
            free_list(cores, core_count);
            free_list(roms, rom_count);
            if (free_core) free(core_path);
            if (free_rom)  free(rom_path);
            return 1;
        }

        if (rom_count == 1) {
            rom_path  = dup_str(roms[0]);
            free_rom  = 1;
            core_path = pick_core_for_rom(rom_path);
            if (!core_path) core_path = dup_str(cores[0]);
            free_core = 1;
        } else {
            int idx = select_from_list("ROM Selector", roms, rom_count);
            if (idx < 0) {
                free_list(cores, core_count);
                free_list(roms, rom_count);
                if (free_core) free(core_path);
                if (free_rom)  free(rom_path);
                return 0;
            }
            rom_path  = dup_str(roms[idx]);
            free_rom  = 1;
            core_path = pick_core_for_rom(rom_path);
            if (!core_path) {
                if (core_count == 1) core_path = dup_str(cores[0]);
                else {
                    int cidx = select_from_list("Core Selector", cores, core_count);
                    if (cidx < 0) {
                        free_list(cores, core_count);
                        free_list(roms, rom_count);
                        if (free_core) free(core_path);
                        if (free_rom)  free(rom_path);
                        return 0;
                    }
                    core_path = dup_str(cores[cidx]);
                }
            }
            free_core = 1;
        }

        free_list(cores, core_count);
        free_list(roms, rom_count);
    }
    if (argc >= 3) {
        core_path = dup_str(argv[1]);
        rom_path  = dup_str(argv[2]);
        free_core = 1; free_rom = 1;
    } else if (argc == 2) {
        rom_path  = dup_str(argv[1]);
        free_rom  = 1;
        core_path = pick_core_for_rom(rom_path);
        if (!core_path) {
            core_path = find_first_or_null("cores", ".dll");
            if (!core_path) core_path = find_first_or_null("cores", ".so");
        }
        free_core = 1;
    } else {
        char path[1200];

        snprintf(path, sizeof(path), "%s/cores", g_root);
        if (!dir_exists(path)) snprintf(path, sizeof(path), "cores");

        int core_count = 0;
        char **cores = list_files(path, ".dll", &core_count);
        if (core_count == 0) {
            free_list(cores, core_count);
            cores = list_files(path, ".so", &core_count);
        }

        if (!cores || core_count == 0) {
            fprintf(stderr, "error: no cores found in %s/\n", path);
            pause_if_click(argc);
            free_list(cores, core_count);
            return 1;
        }

        snprintf(path, sizeof(path), "%s/roms", g_root);
        if (!dir_exists(path)) snprintf(path, sizeof(path), "roms");

        const char *rom_exts[] = { ".gb", ".gbc", ".gba", ".nes", ".sfc", ".smc",
                                   ".md", ".gen", ".sms", ".gg", ".pce" };
        const size_t rom_exts_n = sizeof(rom_exts) / sizeof(rom_exts[0]);

        char **roms = (char**)malloc(sizeof(char*) * MAX_FILES);
        int rom_count = 0;
        if (roms) {
            for (size_t i = 0; i < rom_exts_n; i++) {
                int c = 0;
                char **tmp = list_files(path, rom_exts[i], &c);
                for (int j = 0; j < c && rom_count < MAX_FILES; j++) {
                    roms[rom_count++] = dup_str(tmp[j]);
                }
                free_list(tmp, c);
            }
            if (rom_count > 0) qsort(roms, rom_count, sizeof(char*), cmp_str);
        }

        if (!roms || rom_count == 0) {
            fprintf(stderr, "error: no roms found in %s/\n", path);
            pause_if_click(argc);
            free_list(cores, core_count);
            free_list(roms, rom_count);
            return 1;
        }

        if (rom_count == 1) {
            rom_path  = dup_str(roms[0]);
            free_rom  = 1;
            core_path = pick_core_for_rom(rom_path);
            if (!core_path) core_path = dup_str(cores[0]);
            free_core = 1;
        } else {
            int idx = select_from_list("ROM Selector", roms, rom_count);
            if (idx < 0) {
                free_list(cores, core_count);
                free_list(roms, rom_count);
                return 0;
            }
            rom_path  = dup_str(roms[idx]);
            free_rom  = 1;
            core_path = pick_core_for_rom(rom_path);
            if (!core_path) {
                if (core_count == 1) {
                    core_path = dup_str(cores[0]);
                } else {
                    int cidx = select_from_list("Core Selector", cores, core_count);
                    if (cidx < 0) {
                        free_list(cores, core_count);
                        free_list(roms, rom_count);
                        return 0;
                    }
                    core_path = dup_str(cores[cidx]);
                }
            }
            free_core = 1;
        }

        free_list(cores, core_count);
        free_list(roms, rom_count);
    }

    if (!core_path) {
        fprintf(stderr, "error: no core selected\n");
        pause_if_click(argc);
        if (free_core) free(core_path);
        if (free_rom)  free(rom_path);
        return 1;
    }
    if (!rom_path) {
        fprintf(stderr, "error: no rom selected\n");
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
        pause_if_click(argc); free(rom_data);
        if (free_core) free(core_path);
        if (free_rom)  free(rom_path);
        return 1;
    }

    printf("[launcher] audio_init\n"); fflush(stdout);
    if (!audio_init(av.timing.sample_rate)) {
        pause_if_click(argc); free(rom_data);
        if (free_core) free(core_path);
        if (free_rom)  free(rom_path);
        return 1;
    }

    printf("[launcher] input_init\n"); fflush(stdout);
    if (!input_init()) {
        pause_if_click(argc); free(rom_data);
        if (free_core) free(core_path);
        if (free_rom)  free(rom_path);
        return 1;
    }

    if (g_core.retro_set_controller_port_device)
        g_core.retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);

    printf("[launcher] pixel format: %s\n", pixel_format_name(g_fmt));

    {
        char t[256];
        snprintf(t, sizeof(t), "CoreNest v0.1.0 - %s - %s",
                 sys_info.library_name ? sys_info.library_name : "core",
                 basename_of(rom_path));
        video_set_title(t);
    }

    printf("[launcher] entering main loop\n"); fflush(stdout);

    Uint32 fps_last = SDL_GetTicks();
    int fps_count = 0;
    int paused = 0;

    while (g_running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) g_running = 0;
            else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                case SDLK_ESCAPE: g_running = 0; break;
                case SDLK_F11: video_toggle_fullscreen(); break;
                case SDLK_f:   video_toggle_filter(); break;
                case SDLK_1:   video_set_scale(1); break;
                case SDLK_2:   video_set_scale(2); break;
                case SDLK_3:   video_set_scale(3); break;
                case SDLK_4:   video_set_scale(4); break;
                case SDLK_F1:  g_core.retro_reset(); break;
                case SDLK_p:
                    paused = !paused;
                    printf("[launcher] %s\n", paused ? "paused" : "resumed");
                    fflush(stdout);
                    break;
                }
            }
        }

        if (!paused) { g_core.retro_run(); fps_count++; }
        else SDL_Delay(16);

        Uint32 now = SDL_GetTicks();
        if (now - fps_last >= 1000) {
            char t[256];
            snprintf(t, sizeof(t),
                     "CoreNest v0.1.0 | %s | %d FPS | %dx%s%s",
                     sys_info.library_name ? sys_info.library_name : "core",
                     fps_count, video_get_scale(),
                     video_is_fullscreen() ? " | FULL" : "",
                     paused ? " | PAUSED" : "");
            video_set_title(t);
            fps_count = 0;
            fps_last = now;
        }
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
