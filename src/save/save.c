#include "save.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "libretro.h"

#ifdef _WIN32
  #include <direct.h>
  #define MKDIR(p) _mkdir(p)
#else
  #define MKDIR(p) mkdir((p), 0755)
#endif

static void ensure_dir(const char *path) {
    DIR *d = opendir(path);
    if (d) { closedir(d); return; }
    MKDIR(path);
}

static const char *basename_noext(const char *path, char *out, size_t out_size) {
    const char *p = strrchr(path, '/');
    if (!p) p = strrchr(path, '\\');
    p = p ? p + 1 : path;

    snprintf(out, out_size, "%s", p);

    char *dot = strrchr(out, '.');
    if (dot) *dot = 0;

    return out;
}

void save_make_paths(const char *rom_path, const char *root,
                     char *out_srm, size_t srm_size,
                     char *out_state, size_t state_size)
{
    char name[256];
    basename_noext(rom_path, name, sizeof(name));

    snprintf(out_srm,   srm_size,   "%s/saves/%s.srm",   root, name);
    snprintf(out_state, state_size, "%s/states/%s.state", root, name);
}

bool save_ram_load(core_api_t *core, const char *rom_path, const char *root) {
    if (!core->retro_get_memory_data || !core->retro_get_memory_size) return false;

    void  *data = core->retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    size_t size = core->retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);

    if (!data || size == 0) return false;

    char srm[1200], state[1200];
    save_make_paths(rom_path, root, srm, sizeof(srm), state, sizeof(state));

    FILE *f = fopen(srm, "rb");
    if (!f) {
        printf("[save] no .srm (%s), starting fresh\n", srm);
        return false;
    }

    size_t got = fread(data, 1, size, f);
    fclose(f);

    printf("[save] loaded %zu bytes from %s\n", got, srm);
    return true;
}

bool save_ram_save(core_api_t *core, const char *rom_path, const char *root) {
    if (!core->retro_get_memory_data || !core->retro_get_memory_size) return false;

    void  *data = core->retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    size_t size = core->retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);

    if (!data || size == 0) return false;

    char saves_dir[1200];
    snprintf(saves_dir, sizeof(saves_dir), "%s/saves", root);
    ensure_dir(saves_dir);

    char srm[1200], state[1200];
    save_make_paths(rom_path, root, srm, sizeof(srm), state, sizeof(state));

    FILE *f = fopen(srm, "wb");
    if (!f) {
        fprintf(stderr, "[save] cannot write %s\n", srm);
        return false;
    }

    size_t put = fwrite(data, 1, size, f);
    fclose(f);

    printf("[save] wrote %zu bytes to %s\n", put, srm);
    return true;
}

bool save_state_save(core_api_t *core, const char *rom_path, const char *root) {
    if (!core->retro_serialize_size || !core->retro_serialize) return false;

    size_t size = core->retro_serialize_size();
    if (size == 0) return false;

    void *buf = malloc(size);
    if (!buf) return false;

    if (!core->retro_serialize(buf, size)) {
        free(buf);
        fprintf(stderr, "[save] retro_serialize failed\n");
        return false;
    }

    char states_dir[1200];
    snprintf(states_dir, sizeof(states_dir), "%s/states", root);
    ensure_dir(states_dir);

    char srm[1200], state[1200];
    save_make_paths(rom_path, root, srm, sizeof(srm), state, sizeof(state));

    FILE *f = fopen(state, "wb");
    if (!f) {
        free(buf);
        fprintf(stderr, "[save] cannot write %s\n", state);
        return false;
    }

    fwrite(buf, 1, size, f);
    fclose(f);
    free(buf);

    printf("[save] state saved (%zu bytes) -> %s\n", size, state);
    return true;
}

bool save_state_load(core_api_t *core, const char *rom_path, const char *root) {
    if (!core->retro_unserialize) return false;

    char srm[1200], state[1200];
    save_make_paths(rom_path, root, srm, sizeof(srm), state, sizeof(state));

    FILE *f = fopen(state, "rb");
    if (!f) {
        printf("[save] no state (%s)\n", state);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) { fclose(f); return false; }

    void *buf = malloc((size_t)size);
    if (!buf) { fclose(f); return false; }

    fread(buf, 1, (size_t)size, f);
    fclose(f);

    bool ok = core->retro_unserialize(buf, (size_t)size);
    free(buf);

    printf("[save] state loaded (%ld bytes) <- %s\n", size, state);
    return ok;
}
