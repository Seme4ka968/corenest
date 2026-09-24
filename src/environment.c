#include "environment.h"
#include <stddef.h>

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
        return false;

    case RETRO_ENVIRONMENT_SET_VARIABLES:
    case RETRO_ENVIRONMENT_GET_VARIABLE:
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
    case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
    case RETRO_ENVIRONMENT_GET_LANGUAGE:
    case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
        return false;

    default:
        return false;
    }
}
