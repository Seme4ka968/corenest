#ifndef SAVE_H
#define SAVE_H

#include <stdbool.h>
#include <stddef.h>
#include "core_loader.h"

void save_make_paths(const char *rom_path, const char *root,
                     char *out_srm, size_t srm_size,
                     char *out_state, size_t state_size);

bool save_ram_load(core_api_t *core, const char *rom_path, const char *root);
bool save_ram_save(core_api_t *core, const char *rom_path, const char *root);

bool save_state_save(core_api_t *core, const char *rom_path, const char *root);
bool save_state_load(core_api_t *core, const char *rom_path, const char *root);

#endif
