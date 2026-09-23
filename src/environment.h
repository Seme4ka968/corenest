#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <stdbool.h>
#include "libretro.h"

void env_set_paths(const char *system_dir, const char *save_dir);
bool env_callback(unsigned cmd, void *data);

#endif
