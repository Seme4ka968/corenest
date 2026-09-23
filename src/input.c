#include "input.h"
#include <SDL2/SDL.h>

static const uint8_t *g_keys = NULL;

bool input_init(void) {
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    return true;
}

void input_deinit(void) {
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

void input_poll(void) {
    SDL_PumpEvents();
    g_keys = SDL_GetKeyboardState(NULL);
}

static int16_t key_state(SDL_Scancode sc) {
    return (g_keys && g_keys[sc]) ? 1 : 0;
}

int16_t input_state(unsigned port, unsigned device,
                    unsigned index, unsigned id)
{
    (void)index;
    if (port != 0) return 0;

    if (device == RETRO_DEVICE_JOYPAD) {
        switch (id) {
        case RETRO_DEVICE_ID_JOYPAD_UP:     return key_state(SDL_SCANCODE_UP);
        case RETRO_DEVICE_ID_JOYPAD_DOWN:   return key_state(SDL_SCANCODE_DOWN);
        case RETRO_DEVICE_ID_JOYPAD_LEFT:   return key_state(SDL_SCANCODE_LEFT);
        case RETRO_DEVICE_ID_JOYPAD_RIGHT:  return key_state(SDL_SCANCODE_RIGHT);
        case RETRO_DEVICE_ID_JOYPAD_A:      return key_state(SDL_SCANCODE_X);
        case RETRO_DEVICE_ID_JOYPAD_B:      return key_state(SDL_SCANCODE_Z);
        case RETRO_DEVICE_ID_JOYPAD_X:      return key_state(SDL_SCANCODE_S);
        case RETRO_DEVICE_ID_JOYPAD_Y:      return key_state(SDL_SCANCODE_A);
        case RETRO_DEVICE_ID_JOYPAD_L:      return key_state(SDL_SCANCODE_Q);
        case RETRO_DEVICE_ID_JOYPAD_R:      return key_state(SDL_SCANCODE_W);
        case RETRO_DEVICE_ID_JOYPAD_START:  return key_state(SDL_SCANCODE_RETURN);
        case RETRO_DEVICE_ID_JOYPAD_SELECT: return key_state(SDL_SCANCODE_RSHIFT);
        default: return 0;
        }
    }
    return 0;
}
