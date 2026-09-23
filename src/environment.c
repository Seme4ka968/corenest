#include "environment.h"
#include <stdio.h>
#include <string.h>

static const char *g_system_dir = ".";
static const char *g_save_dir   = ".";

void env_set_paths(const char *system_dir, const char *save_dir) {
    if (system_dir) g_system_dir = system_dir;
    if (save_dir)   g_save_dir   = save_dir;
}

bool env_callback(unsigned cmd, void *data) {
    switch (cmd) {
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        *(const char**)data = g_system_dir;
        return true;

    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *(const char**)data = g_save_dir;
        return true;

    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        *(bool*)data = true;
        return true;

    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        return true;

    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        return true;

    default:
        return false;
    }
}
